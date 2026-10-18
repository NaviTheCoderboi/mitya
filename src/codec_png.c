#ifdef MITYA_ENABLE_PNG

#include <png.h>
#include <stdlib.h>
#include <string.h>

#include "mitya/image.h"

static int png_is_little_endian(void) {
    const uint16_t one = 1;
    return *(const uint8_t*)&one == 1;
}

static void png_error_cb(png_structp png, png_const_charp msg) {
    (void)msg;
    png_longjmp(png, 1);
}

static void png_warning_cb(png_structp png, png_const_charp msg) {
    (void)png;
    (void)msg;
}

typedef struct {
    const uint8_t* data;
    size_t size;
    size_t pos;
} png_mem_reader_t;

static void png_read_cb(png_structp png, png_bytep out, size_t len) {
    png_mem_reader_t* r = (png_mem_reader_t*)png_get_io_ptr(png);

    if (len > r->size - r->pos) png_error(png, "unexpected end of data");

    memcpy(out, r->data + r->pos, len);
    r->pos += len;
}

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} png_mem_writer_t;

static void png_write_cb(png_structp png, png_bytep in, size_t len) {
    png_mem_writer_t* w = (png_mem_writer_t*)png_get_io_ptr(png);

    if (len > SIZE_MAX - w->size) png_error(png, "size overflow");

    size_t needed = w->size + len;
    if (needed > w->capacity) {
        size_t new_cap = w->capacity ? w->capacity : 4096;
        while (new_cap < needed) {
            if (new_cap > SIZE_MAX / 2) {
                new_cap = needed;
                break;
            }
            new_cap *= 2;
        }

        uint8_t* p = (uint8_t*)realloc(w->data, new_cap);
        if (!p) png_error(png, "out of memory");

        w->data = p;
        w->capacity = new_cap;
    }

    memcpy(w->data + w->size, in, len);
    w->size += len;
}

static void png_flush_cb(png_structp png) { (void)png; }

static mitya_image_result_t png_probe(const void* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 8) return MITYA_IMAGE_ERROR_DECODE_FAILED;

    return png_sig_cmp((png_const_bytep)buffer, 0, 8) == 0
               ? MITYA_IMAGE_SUCCESS
               : MITYA_IMAGE_ERROR_DECODE_FAILED;
}

static mitya_image_result_t png_decode(const void* buffer, size_t buffer_size,
                                       mitya_image_t* image) {
    if (png_probe(buffer, buffer_size) != MITYA_IMAGE_SUCCESS)
        return MITYA_IMAGE_ERROR_DECODE_FAILED;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                             png_error_cb, png_warning_cb);
    if (!png) return MITYA_IMAGE_ERROR_OUT_OF_MEMORY;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);

        return MITYA_IMAGE_ERROR_OUT_OF_MEMORY;
    }

    png_bytep* volatile rows = NULL;
    volatile int image_created = 0;

    if (setjmp(png_jmpbuf(png))) {
        free((void*)rows);

        if (image_created) mitya_image_destroy(image);

        png_destroy_read_struct(&png, &info, NULL);

        return MITYA_IMAGE_ERROR_DECODE_FAILED;
    }

    png_mem_reader_t reader = {(const uint8_t*)buffer, buffer_size, 0};
    png_set_read_fn(png, &reader, png_read_cb);

    png_read_info(png, info);

    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);
    int bit_depth = png_get_bit_depth(png, info);
    int color_type = png_get_color_type(png, info);

    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);

    if (bit_depth == 16 && png_is_little_endian()) png_set_swap(png);

    png_set_interlace_handling(png);
    png_read_update_info(png, info);

    int out_depth = png_get_bit_depth(png, info);
    int channels = png_get_channels(png, info);

    if (channels < 1 || channels > 4 || (out_depth != 8 && out_depth != 16))
        png_error(png, "unsupported format");

    mitya_image_pixel_type_t type =
        (out_depth == 16) ? MITYA_IMAGE_TYPE_U16 : MITYA_IMAGE_TYPE_U8;

    mitya_image_result_t res =
        mitya_image_create(image, width, height, (uint8_t)channels, type);
    if (res != MITYA_IMAGE_SUCCESS) png_error(png, "image allocation failed");
    image_created = 1;

    if (png_get_rowbytes(png, info) > image->row_stride)
        png_error(png, "row size mismatch");

    rows = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (!rows) png_error(png, "out of memory");

    for (png_uint_32 y = 0; y < height; ++y)
        rows[y] = (png_bytep)image->pixels + (size_t)y * image->row_stride;

    png_read_image(png, rows);
    png_read_end(png, NULL);

    free((void*)rows);
    png_destroy_read_struct(&png, &info, NULL);

    return MITYA_IMAGE_SUCCESS;
}

static mitya_image_result_t png_encode(const mitya_image_t* image,
                                       void** buffer, size_t* buffer_size) {
    int bit_depth;
    switch (image->type) {
        case MITYA_IMAGE_TYPE_U8:
            bit_depth = 8;
            break;
        case MITYA_IMAGE_TYPE_U16:
            bit_depth = 16;
            break;
        default:
            return MITYA_IMAGE_ERROR_ENCODE_FAILED;
    }

    int color_type;
    switch (image->channels) {
        case 1:
            color_type = PNG_COLOR_TYPE_GRAY;
            break;
        case 2:
            color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
            break;
        case 3:
            color_type = PNG_COLOR_TYPE_RGB;
            break;
        case 4:
            color_type = PNG_COLOR_TYPE_RGB_ALPHA;
            break;
        default:
            return MITYA_IMAGE_ERROR_ENCODE_FAILED;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                              png_error_cb, png_warning_cb);

    if (!png) return MITYA_IMAGE_ERROR_OUT_OF_MEMORY;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);

        return MITYA_IMAGE_ERROR_OUT_OF_MEMORY;
    }

    png_mem_writer_t writer = {NULL, 0, 0};

    if (setjmp(png_jmpbuf(png))) {
        free(writer.data);
        png_destroy_write_struct(&png, &info);

        return MITYA_IMAGE_ERROR_ENCODE_FAILED;
    }

    png_set_write_fn(png, &writer, png_write_cb, png_flush_cb);

    png_set_IHDR(png, info, image->width, image->height, bit_depth, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, info);

    if (bit_depth == 16 && png_is_little_endian()) png_set_swap(png);

    for (uint32_t y = 0; y < image->height; ++y) {
        const uint8_t* row =
            (const uint8_t*)image->pixels + (size_t)y * image->row_stride;
        png_write_row(png, (png_const_bytep)row);
    }

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);

    *buffer = writer.data;
    *buffer_size = writer.size;

    return MITYA_IMAGE_SUCCESS;
}

const mitya_codec_t mitya_codec_png = {png_probe, png_encode, png_decode};

#endif /* MITYA_ENABLE_PNG */