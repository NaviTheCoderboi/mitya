#ifdef MITYA_ENABLE_PNM

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mitya/image.h"

typedef struct {
    const uint8_t* data;
    size_t size;
    size_t pos;
} pnm_reader_t;

static int pnm_is_ws(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f';
}

static int pnm_mul(size_t a, size_t b, size_t* out) {
    if (a != 0 && b > SIZE_MAX / a) return 0;

    *out = a * b;

    return 1;
}

static void pnm_skip_ws(pnm_reader_t* r) {
    while (r->pos < r->size) {
        uint8_t c = r->data[r->pos];

        if (c == '#') {
            while (r->pos < r->size && r->data[r->pos] != '\n') r->pos++;
        } else if (pnm_is_ws(c)) {
            r->pos++;
        } else {
            break;
        }
    }
}

static int pnm_read_uint(pnm_reader_t* r, uint32_t max, uint32_t* out) {
    pnm_skip_ws(r);

    if (r->pos >= r->size || r->data[r->pos] < '0' || r->data[r->pos] > '9')
        return 0;

    uint64_t v = 0;
    while (r->pos < r->size && r->data[r->pos] >= '0' &&
           r->data[r->pos] <= '9') {
        v = v * 10 + (uint64_t)(r->data[r->pos] - '0');

        if (v > max) return 0;

        r->pos++;
    }

    *out = (uint32_t)v;
    return 1;
}

static mitya_image_result_t pnm_probe(const void* buffer, size_t buffer_size) {
    const uint8_t* d = (const uint8_t*)buffer;

    if (!d || buffer_size < 3 || d[0] != 'P')
        return MITYA_IMAGE_ERROR_DECODE_FAILED;

    if ((d[1] == '2' || d[1] == '3' || d[1] == '5' || d[1] == '6') &&
        pnm_is_ws(d[2]))
        return MITYA_IMAGE_SUCCESS;

    return MITYA_IMAGE_ERROR_DECODE_FAILED;
}

static mitya_image_result_t pnm_decode(const void* buffer, size_t buffer_size,
                                       mitya_image_t* image) {
    if (pnm_probe(buffer, buffer_size) != MITYA_IMAGE_SUCCESS)
        return MITYA_IMAGE_ERROR_DECODE_FAILED;

    const uint8_t* d = (const uint8_t*)buffer;
    int binary = (d[1] == '5' || d[1] == '6');
    uint8_t channels = (d[1] == '3' || d[1] == '6') ? 3 : 1;

    pnm_reader_t r = {d, buffer_size, 2};
    uint32_t width, height, maxval;

    if (!pnm_read_uint(&r, UINT32_MAX, &width) ||
        !pnm_read_uint(&r, UINT32_MAX, &height) ||
        !pnm_read_uint(&r, 65535, &maxval) || width == 0 || height == 0 ||
        maxval == 0)
        return MITYA_IMAGE_ERROR_DECODE_FAILED;

    if (binary) {
        if (r.pos >= r.size || !pnm_is_ws(r.data[r.pos]))
            return MITYA_IMAGE_ERROR_DECODE_FAILED;

        r.pos++;
    }

    size_t samples;
    if (!pnm_mul(width, height, &samples) ||
        !pnm_mul(samples, channels, &samples))
        return MITYA_IMAGE_ERROR_DECODE_FAILED;

    size_t in_bps = (maxval > 255) ? 2 : 1;
    size_t remaining = r.size - r.pos;

    if (binary) {
        size_t bytes;

        if (!pnm_mul(samples, in_bps, &bytes) || bytes > remaining)
            return MITYA_IMAGE_ERROR_DECODE_FAILED;
    } else if (samples > remaining) {
        return MITYA_IMAGE_ERROR_DECODE_FAILED;
    }

    int out16 = maxval > 255;
    uint32_t out_max = out16 ? 65535u : 255u;

    mitya_image_result_t res =
        mitya_image_create(image, width, height, channels,
                           out16 ? MITYA_IMAGE_TYPE_U16 : MITYA_IMAGE_TYPE_U8);
    if (res != MITYA_IMAGE_SUCCESS)
        return (res == MITYA_IMAGE_ERROR_INVALID_ARGUMENT)
                   ? MITYA_IMAGE_ERROR_DECODE_FAILED
                   : res;

    size_t samples_per_row = (size_t)width * channels;

    for (uint32_t y = 0; y < height; ++y) {
        uint8_t* row = (uint8_t*)image->pixels + (size_t)y * image->row_stride;

        for (size_t i = 0; i < samples_per_row; ++i) {
            uint32_t v;

            if (binary) {
                if (in_bps == 2) {
                    v = ((uint32_t)r.data[r.pos] << 8) | r.data[r.pos + 1];
                    r.pos += 2;
                } else {
                    v = r.data[r.pos++];
                }
            } else if (!pnm_read_uint(&r, 65535, &v)) {
                mitya_image_destroy(image);

                return MITYA_IMAGE_ERROR_DECODE_FAILED;
            }

            if (v > maxval) v = maxval;

            if (maxval != out_max)
                v = (uint32_t)(((uint64_t)v * out_max + maxval / 2) / maxval);

            if (out16) {
                uint16_t v16 = (uint16_t)v;
                memcpy(row + i * 2, &v16, 2);
            } else {
                row[i] = (uint8_t)v;
            }
        }
    }

    return MITYA_IMAGE_SUCCESS;
}

static mitya_image_result_t pnm_encode(const mitya_image_t* image,
                                       void** buffer, size_t* buffer_size) {
    char magic;
    if (image->channels == 1)
        magic = '5';
    else if (image->channels == 3)
        magic = '6';
    else
        return MITYA_IMAGE_ERROR_ENCODE_FAILED;

    size_t bps;
    unsigned long maxval;
    switch (image->type) {
        case MITYA_IMAGE_TYPE_U8:
            bps = 1;
            maxval = 255;
            break;
        case MITYA_IMAGE_TYPE_U16:
            bps = 2;
            maxval = 65535;
            break;
        default:
            return MITYA_IMAGE_ERROR_ENCODE_FAILED;
    }

    size_t row_bytes, data_bytes;
    if (!pnm_mul(image->width, (size_t)image->channels * bps, &row_bytes) ||
        image->row_stride < row_bytes ||
        !pnm_mul(row_bytes, image->height, &data_bytes)) {
        return MITYA_IMAGE_ERROR_ENCODE_FAILED;
    }

    char header[64];
    int hlen = snprintf(header, sizeof header, "P%c\n%lu %lu\n%lu\n", magic,
                        (unsigned long)image->width,
                        (unsigned long)image->height, maxval);
    if (hlen <= 0 || (size_t)hlen >= sizeof header)
        return MITYA_IMAGE_ERROR_ENCODE_FAILED;

    if (data_bytes > SIZE_MAX - (size_t)hlen)
        return MITYA_IMAGE_ERROR_ENCODE_FAILED;

    uint8_t* out = (uint8_t*)malloc((size_t)hlen + data_bytes);
    if (!out) return MITYA_IMAGE_ERROR_OUT_OF_MEMORY;

    memcpy(out, header, (size_t)hlen);
    uint8_t* dst = out + hlen;

    size_t samples_per_row = (size_t)image->width * image->channels;

    for (uint32_t y = 0; y < image->height; ++y) {
        const uint8_t* src =
            (const uint8_t*)image->pixels + (size_t)y * image->row_stride;

        if (bps == 1) {
            memcpy(dst, src, row_bytes);
            dst += row_bytes;
        } else {
            for (size_t i = 0; i < samples_per_row; ++i) {
                uint16_t v;
                memcpy(&v, src + i * 2, 2);
                *dst++ = (uint8_t)(v >> 8);
                *dst++ = (uint8_t)(v & 0xFF);
            }
        }
    }

    *buffer = out;
    *buffer_size = (size_t)hlen + data_bytes;
    return MITYA_IMAGE_SUCCESS;
}

const mitya_codec_t mitya_codec_pnm = {pnm_probe, pnm_encode, pnm_decode};

#endif  // MITYA_ENABLE_PNM