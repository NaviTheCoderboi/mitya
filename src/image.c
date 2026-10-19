#include "mitya/image.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mitya/codecs.h"

#ifdef MITYA_USER_CODECS_HEADER

#define MITYA_X_EXTERN(name, symbol) extern const mitya_codec_t symbol;
MITYA_USER_CODECS(MITYA_X_EXTERN)
#undef MITYA_X_EXTERN

#define MITYA_X_POINTER(name, symbol) &symbol,
static const mitya_codec_t* const _mitya_user_codecs[] = {
    MITYA_USER_CODECS(MITYA_X_POINTER) NULL};
#undef MITYA_X_POINTER

#define MITYA_USER_CODEC_COUNT \
    (sizeof(_mitya_user_codecs) / sizeof(_mitya_user_codecs[0]) - 1)

#endif

// a empty codec just to make sure the array is not empty, so that the code
// compiles without warnings
static mitya_image_result_t _mitya_empty_probe(const void* buffer,
                                               size_t buffer_size) {
    (void)buffer;
    (void)buffer_size;
    return MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT;
}

static mitya_image_result_t _mitya_empty_decode(const void* buffer,
                                                size_t buffer_size,
                                                mitya_image_t* image) {
    (void)buffer;
    (void)buffer_size;
    (void)image;
    return MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT;
}

static mitya_image_result_t _mitya_empty_encode(const mitya_image_t* image,
                                                void** buffer,
                                                size_t* buffer_size) {
    (void)image;
    (void)buffer;
    (void)buffer_size;
    return MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT;
}

const mitya_codec_t* const mitya_codecs[] = {
    [MITYA_IMAGE_FORMAT_UNKNOWN] =
        &(mitya_codec_t){.probe = _mitya_empty_probe,
                         .decode = _mitya_empty_decode,
                         .encode = _mitya_empty_encode},
#ifdef MITYA_ENABLE_PNG
    [MITYA_IMAGE_FORMAT_PNG] = &mitya_codec_png,
#endif
#ifdef MITYA_ENABLE_PNM
    [MITYA_IMAGE_FORMAT_PNM] = &mitya_codec_pnm,
#endif
};

#define MITYA_CODEC_COUNT (sizeof(mitya_codecs) / sizeof(mitya_codecs[0]))

static int _mitya_type_is_float(mitya_image_pixel_type_t type) {
    return type == MITYA_IMAGE_TYPE_F32 || type == MITYA_IMAGE_TYPE_F64;
}

static size_t _mitya_bytes_per_pixel(mitya_image_pixel_type_t type) {
    switch (type) {
        case MITYA_IMAGE_TYPE_U8:
            return 1;
        case MITYA_IMAGE_TYPE_U16:
            return 2;
        case MITYA_IMAGE_TYPE_U32:
            return 4;
        case MITYA_IMAGE_TYPE_F32:
            return 4;
        case MITYA_IMAGE_TYPE_F64:
            return 8;
        default:
            return 0;
    }
}

static double _mitya_read_pixel_as_double(const void* p,
                                          mitya_image_pixel_type_t type) {
    if (!p) return 0.0;

    switch (type) {
        case MITYA_IMAGE_TYPE_U8:
            return *(const uint8_t*)p / 255.0;
        case MITYA_IMAGE_TYPE_U16:
            return *(const uint16_t*)p / 65535.0;
        case MITYA_IMAGE_TYPE_U32:
            return *(const uint32_t*)p / 4294967295.0;
        case MITYA_IMAGE_TYPE_F32:
            return *(const float*)p;
        case MITYA_IMAGE_TYPE_F64:
            return *(const double*)p;
        default:
            return 0.0;
    }
}

static void _mitya_write_pixel_from_double(void* p,
                                           mitya_image_pixel_type_t type,
                                           double v) {
    if (!_mitya_type_is_float(type)) {
        if (!(v >= 0.0)) {
            v = 0.0;
        } else if (v > 1.0) {
            v = 1.0;
        }
    }

    switch (type) {
        case MITYA_IMAGE_TYPE_U8:
            *(uint8_t*)p = (uint8_t)(v * 255.0 + 0.5);
            break;
        case MITYA_IMAGE_TYPE_U16:
            *(uint16_t*)p = (uint16_t)(v * 65535.0 + 0.5);
            break;
        case MITYA_IMAGE_TYPE_U32:
            *(uint32_t*)p = (uint32_t)(v * 4294967295.0 + 0.5);
            break;
        case MITYA_IMAGE_TYPE_F32:
            *(float*)p = (float)v;
            break;
        case MITYA_IMAGE_TYPE_F64:
            *(double*)p = v;
            break;
        default:
            break;
    }
}

static mitya_image_result_t _mitya_convert_pixels(const mitya_image_t* src,
                                                  mitya_image_t* dst) {
    if (!src || !dst || !src->pixels || !dst->pixels)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    if (src->width != dst->width || src->height != dst->height ||
        src->channels != dst->channels)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    size_t src_bpp = _mitya_bytes_per_pixel(src->type);
    size_t dst_bpp = _mitya_bytes_per_pixel(dst->type);
    if (src_bpp == 0 || dst_bpp == 0) return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    size_t samples_per_row = (size_t)src->width * src->channels;

    if (samples_per_row > SIZE_MAX / 8)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    if (src->row_stride < samples_per_row * src_bpp ||
        dst->row_stride < samples_per_row * dst_bpp)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    const uint8_t* src_base = (const uint8_t*)src->pixels;
    uint8_t* dst_base = (uint8_t*)dst->pixels;

    for (uint32_t y = 0; y < src->height; ++y) {
        const uint8_t* s = src_base + (size_t)y * src->row_stride;
        uint8_t* d = dst_base + (size_t)y * dst->row_stride;

        for (size_t i = 0; i < samples_per_row; ++i) {
            double v = _mitya_read_pixel_as_double(s + i * src_bpp, src->type);
            _mitya_write_pixel_from_double(d + i * dst_bpp, dst->type, v);
        }
    }

    return MITYA_IMAGE_SUCCESS;
}

static const mitya_codec_t* _mitya_find_codec(mitya_image_format_t format) {
    if (format == MITYA_IMAGE_FORMAT_UNKNOWN) return NULL;

    if ((size_t)format >= MITYA_CODEC_COUNT) return NULL;

#ifdef MITYA_USER_CODECS_HEADER
    if ((int)format > (int)MITYA_IMAGE_FORMAT_USER_BASE) {
        size_t i =
            (size_t)((int)format - (int)MITYA_IMAGE_FORMAT_USER_BASE - 1);

        return i < MITYA_USER_CODEC_COUNT ? _mitya_user_codecs[i] : NULL;
    }
#endif

    return mitya_codecs[format];
}

static const mitya_codec_t* _mitya_probe_codecs(const void* buffer,
                                                size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) return NULL;

    for (size_t i = 1; i < MITYA_CODEC_COUNT; i++) {
        const mitya_codec_t* codec = mitya_codecs[i];

        if (codec && codec->probe &&
            codec->probe(buffer, buffer_size) == MITYA_IMAGE_SUCCESS)
            return codec;
    }

#ifdef MITYA_USER_CODECS_HEADER
    for (size_t i = 0; i < MITYA_USER_CODEC_COUNT; i++) {
        const mitya_codec_t* codec = _mitya_user_codecs[i];

        if (codec && codec->probe &&
            codec->probe(buffer, buffer_size) == MITYA_IMAGE_SUCCESS)
            return codec;
    }
#endif

    return NULL;
}

mitya_image_result_t mitya_image_create(mitya_image_t* image, uint32_t width,
                                        uint32_t height, uint8_t channels,
                                        mitya_image_pixel_type_t type) {
    if (!image || width == 0 || height == 0 || channels == 0) {
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;
    }

    memset(image, 0, sizeof(*image));

    size_t bpp = _mitya_bytes_per_pixel(type);
    if (bpp == 0) return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    size_t pixel_size = bpp * (size_t)channels;

    if ((size_t)width > SIZE_MAX / pixel_size)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    size_t row_stride = (size_t)width * pixel_size;

    if ((size_t)height > SIZE_MAX / row_stride)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    size_t data_size = row_stride * (size_t)height;

    void* pixels = calloc(1, data_size);
    if (!pixels) return MITYA_IMAGE_ERROR_OUT_OF_MEMORY;

    image->width = width;
    image->height = height;
    image->channels = channels;
    image->type = type;
    image->row_stride = row_stride;
    image->plane_stride = row_stride;
    image->pixels = pixels;
    image->data_size = data_size;

    return MITYA_IMAGE_SUCCESS;
}

void mitya_image_destroy(mitya_image_t* image) {
    if (!image) return;

    free(image->pixels);
    memset(image, 0, sizeof(*image));
}

mitya_image_result_t mitya_image_load_from_buffer(
    mitya_image_t* image, const void* buffer, size_t buffer_size,
    mitya_image_format_t format, mitya_image_pixel_type_t desired_type) {
    if (!image || !buffer || buffer_size == 0)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    if (_mitya_bytes_per_pixel(desired_type) == 0)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    memset(image, 0, sizeof(*image));

    const mitya_codec_t* codec;
    if (format == MITYA_IMAGE_FORMAT_UNKNOWN) {
        codec = _mitya_probe_codecs(buffer, buffer_size);

        if (!codec) return MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT;
    } else {
        codec = _mitya_find_codec(format);

        if (!codec) return MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT;
    }

    if (!codec->decode) return MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT;

    mitya_image_result_t res = codec->decode(buffer, buffer_size, image);
    if (res != MITYA_IMAGE_SUCCESS) return res;

    if (desired_type != image->type) {
        mitya_image_t converted;

        res = mitya_image_create(&converted, image->width, image->height,
                                 image->channels, desired_type);

        if (res != MITYA_IMAGE_SUCCESS) {
            mitya_image_destroy(image);

            return res;
        }

        _mitya_convert_pixels(image, &converted);
        mitya_image_destroy(image);
        *image = converted;
    }

    return MITYA_IMAGE_SUCCESS;
}

mitya_image_result_t mitya_image_load_from_file(
    mitya_image_t* image, const char* filename, mitya_image_format_t format,
    mitya_image_pixel_type_t desired_type) {
    if (!image || !filename) return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    FILE* fp = fopen(filename, "rb");
    if (!fp)
        return (errno == ENOENT) ? MITYA_IMAGE_ERROR_FILE_NOT_FOUND
                                 : MITYA_IMAGE_ERROR_IO;

    mitya_image_result_t res = MITYA_IMAGE_ERROR_IO;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return res;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return res;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return res;
    }

    void* buffer = malloc((size_t)size);
    if (!buffer) {
        res = MITYA_IMAGE_ERROR_OUT_OF_MEMORY;

        fclose(fp);
        return res;
    }

    if (fread(buffer, 1, (size_t)size, fp) != (size_t)size) {
        free(buffer);

        fclose(fp);
        return res;
    }

    res = mitya_image_load_from_buffer(image, buffer, (size_t)size, format,
                                       desired_type);
    free(buffer);

    fclose(fp);
    return res;
}

mitya_image_result_t mitya_image_save_to_buffer(
    const mitya_image_t* image, void** buffer, size_t* buffer_size,
    mitya_image_format_t format, mitya_image_pixel_type_t desired_type) {
    if (!image || !image->pixels || image->width == 0 || image->height == 0 ||
        image->channels == 0 || !buffer || !buffer_size)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    if (_mitya_bytes_per_pixel(desired_type) == 0)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    *buffer = NULL;
    *buffer_size = 0;

    const mitya_codec_t* codec = _mitya_find_codec(format);
    if (!codec || !codec->encode) return MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT;

    if (desired_type == image->type)
        return codec->encode(image, buffer, buffer_size);

    mitya_image_t tmp;
    mitya_image_result_t res = mitya_image_create(
        &tmp, image->width, image->height, image->channels, desired_type);

    if (res != MITYA_IMAGE_SUCCESS) return res;

    _mitya_convert_pixels(image, &tmp);
    res = codec->encode(&tmp, buffer, buffer_size);
    mitya_image_destroy(&tmp);

    return res;
}

mitya_image_result_t mitya_image_save_to_file(
    const mitya_image_t* image, const char* filename,
    mitya_image_format_t format, mitya_image_pixel_type_t desired_type) {
    if (!filename) return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    void* buffer = NULL;
    size_t buffer_size = 0;

    mitya_image_result_t res = mitya_image_save_to_buffer(
        image, &buffer, &buffer_size, format, desired_type);

    if (res != MITYA_IMAGE_SUCCESS) return res;

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        mitya_buffer_free(&buffer);

        return MITYA_IMAGE_ERROR_IO;
    }

    if (buffer_size > 0 && fwrite(buffer, 1, buffer_size, fp) != buffer_size) {
        fclose(fp);
        mitya_buffer_free(&buffer);

        return MITYA_IMAGE_ERROR_IO;
    }

    fclose(fp);
    mitya_buffer_free(&buffer);

    return MITYA_IMAGE_SUCCESS;
}

void mitya_buffer_free(void** buffer) {
    if (!buffer || !*buffer) return;
    free(*buffer);
    *buffer = NULL;
}

uint32_t mitya_image_width(const mitya_image_t* image) {
    return image ? image->width : 0;
}

uint32_t mitya_image_height(const mitya_image_t* image) {
    return image ? image->height : 0;
}

uint8_t mitya_image_channels(const mitya_image_t* image) {
    return image ? image->channels : 0;
}

mitya_image_pixel_type_t mitya_image_pixel_type(const mitya_image_t* image) {
    return image ? image->type : MITYA_IMAGE_TYPE_U8;
}