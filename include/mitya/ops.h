#ifndef MITYA_OPS_H
#define MITYA_OPS_H

#define PI 3.14159265358979323846

#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t x;
    int32_t y;
} mitya_point_t;

typedef enum { MITYA_INTERP_NEAREST, MITYA_INTERP_BILINEAR } mitya_interp_t;

typedef struct {
    double angle_deg;
    mitya_interp_t interp;
    int expand;
    double fill[4];
} mitya_rotate_opts_t;

mitya_image_result_t mitya_image_crop(const mitya_image_t* src,
                                      mitya_image_t* dst, mitya_point_t p0,
                                      mitya_point_t p1);

mitya_image_result_t mitya_image_rotate90(const mitya_image_t* src,
                                          mitya_image_t* dst,
                                          int quarter_turns);

mitya_image_result_t mitya_image_rotate(const mitya_image_t* src,
                                        mitya_image_t* dst,
                                        const mitya_rotate_opts_t* opts);

mitya_image_result_t mitya_image_resize(const mitya_image_t* src,
                                        mitya_image_t* dst, uint32_t w,
                                        uint32_t h, mitya_interp_t interp);

// mitya_image_result_t mitya_image_flip_horizontal(mitya_image_t* image);
// mitya_image_result_t mitya_image_flip_vertical(mitya_image_t* image);

#ifdef __cplusplus
}
#endif

#endif /* MITYA_OPS_H */