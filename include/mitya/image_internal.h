#ifndef MITYA_IMAGE_INTERNAL_H
#define MITYA_IMAGE_INTERNAL_H

#include <stdint.h>
#include <string.h>

#include "mitya/image.h"

static inline const uint8_t* px_at(const mitya_image_t* im, size_t px,
                                   uint32_t x, uint32_t y) {
    return (const uint8_t*)im->pixels + (size_t)y * im->row_stride + x * px;
}

static inline uint8_t* px_at_mut(mitya_image_t* im, size_t px, uint32_t x,
                                 uint32_t y) {
    return (uint8_t*)im->pixels + (size_t)y * im->row_stride + x * px;
}

static inline size_t mitya_sample_size(mitya_image_pixel_type_t type) {
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

static inline double mitya_read_sample(const void* p,
                                       mitya_image_pixel_type_t t) {
    switch (t) {
        case MITYA_IMAGE_TYPE_U8: {
            uint8_t v;
            memcpy(&v, p, 1);
            return v / 255.0;
        }
        case MITYA_IMAGE_TYPE_U16: {
            uint16_t v;
            memcpy(&v, p, 2);
            return v / 65535.0;
        }
        case MITYA_IMAGE_TYPE_U32: {
            uint32_t v;
            memcpy(&v, p, 4);
            return v / 4294967295.0;
        }
        case MITYA_IMAGE_TYPE_F32: {
            float v;
            memcpy(&v, p, 4);
            return v;
        }
        case MITYA_IMAGE_TYPE_F64: {
            double v;
            memcpy(&v, p, 8);
            return v;
        }
        default:
            return 0.0;
    }
}

static inline void mitya_write_sample(void* p, mitya_image_pixel_type_t t,
                                      double v) {
    if (t != MITYA_IMAGE_TYPE_F32 && t != MITYA_IMAGE_TYPE_F64) {
        if (!(v >= 0.0))
            v = 0.0;
        else if (v > 1.0)
            v = 1.0;
    }

    switch (t) {
        case MITYA_IMAGE_TYPE_U8: {
            uint8_t o = (uint8_t)(v * 255.0 + 0.5);
            memcpy(p, &o, 1);
            break;
        }
        case MITYA_IMAGE_TYPE_U16: {
            uint16_t o = (uint16_t)(v * 65535.0 + 0.5);
            memcpy(p, &o, 2);
            break;
        }
        case MITYA_IMAGE_TYPE_U32: {
            uint32_t o = (uint32_t)(v * 4294967295.0 + 0.5);
            memcpy(p, &o, 4);
            break;
        }
        case MITYA_IMAGE_TYPE_F32: {
            float o = (float)v;
            memcpy(p, &o, 4);
            break;
        }
        case MITYA_IMAGE_TYPE_F64: {
            double o = v;
            memcpy(p, &o, 8);
            break;
        }
        default:
            break;
    }
}

#endif  // MITYA_IMAGE_INTERNAL_H