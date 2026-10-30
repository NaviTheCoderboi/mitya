#ifndef MITYA_IMAGE_H
#define MITYA_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef MITYA_USER_CODECS_HEADER
#include MITYA_USER_CODECS_HEADER
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MITYA_IMAGE_SUCCESS,

    MITYA_IMAGE_ERROR_INVALID_ARGUMENT,
    MITYA_IMAGE_ERROR_FILE_NOT_FOUND,
    MITYA_IMAGE_ERROR_IO,
    MITYA_IMAGE_ERROR_DECODE_FAILED,
    MITYA_IMAGE_ERROR_ENCODE_FAILED,
    MITYA_IMAGE_ERROR_OUT_OF_MEMORY,
    MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT
} mitya_image_result_t;

typedef enum {
    MITYA_IMAGE_TYPE_U8,
    MITYA_IMAGE_TYPE_U16,
    MITYA_IMAGE_TYPE_U32,
    MITYA_IMAGE_TYPE_F32,
    MITYA_IMAGE_TYPE_F64
} mitya_image_pixel_type_t;

typedef enum {
    MITYA_IMAGE_FORMAT_UNKNOWN,
    MITYA_IMAGE_FORMAT_PNG,
    MITYA_IMAGE_FORMAT_PNM,

    MITYA_IMAGE_FORMAT_USER_BASE = 0x1000,
#ifdef MITYA_USER_CODECS_HEADER
#define MITYA_X_FORMAT_ENUM(name, symbol) MITYA_IMAGE_FORMAT_##name,
    MITYA_USER_CODECS(MITYA_X_FORMAT_ENUM)
#undef MITYA_X_FORMAT_ENUM
#endif

} mitya_image_format_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    mitya_image_pixel_type_t type;

    size_t row_stride;

    void* pixels;
    size_t data_size;
} mitya_image_t;

typedef struct {
    mitya_image_result_t (*probe)(const void* buffer, size_t buffer_size);
    mitya_image_result_t (*encode)(const mitya_image_t* image, void** buffer,
                                   size_t* buffer_size);
    mitya_image_result_t (*decode)(const void* buffer, size_t buffer_size,
                                   mitya_image_t* image);
} mitya_codec_t;

extern const mitya_codec_t* const mitya_codecs[];

mitya_image_result_t mitya_image_create(mitya_image_t* image, uint32_t width,
                                        uint32_t height, uint8_t channels,
                                        mitya_image_pixel_type_t type);
void mitya_image_destroy(mitya_image_t* image);

mitya_image_result_t mitya_image_load_from_buffer(
    mitya_image_t* image, const void* buffer, size_t buffer_size,
    mitya_image_format_t format, mitya_image_pixel_type_t desired_type);
mitya_image_result_t mitya_image_load_from_file(
    mitya_image_t* image, const char* filename, mitya_image_format_t format,
    mitya_image_pixel_type_t desired_type);

mitya_image_result_t mitya_image_save_to_buffer(
    const mitya_image_t* image, void** buffer, size_t* buffer_size,
    mitya_image_format_t format, mitya_image_pixel_type_t desired_type);
mitya_image_result_t mitya_image_save_to_file(
    const mitya_image_t* image, const char* filename,
    mitya_image_format_t format, mitya_image_pixel_type_t desired_type);

void mitya_buffer_free(void** buffer);

uint32_t mitya_image_width(const mitya_image_t* image);
uint32_t mitya_image_height(const mitya_image_t* image);
uint8_t mitya_image_channels(const mitya_image_t* image);
mitya_image_pixel_type_t mitya_image_pixel_type(const mitya_image_t* image);
bool mitya_image_valid(const mitya_image_t* image);

#ifdef __cplusplus
}
#endif

#endif /* MITYA_IMAGE_H */