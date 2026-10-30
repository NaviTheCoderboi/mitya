#include "mitya/ops.h"

#include <math.h>
#include <string.h>

#include "mitya/image.h"
#include "mitya/image_internal.h"

mitya_image_result_t mitya_image_crop(const mitya_image_t* src,
                                      mitya_image_t* dst, mitya_point_t p0,
                                      mitya_point_t p1) {
    if (!mitya_image_valid(src) || !dst || (const void*)src == (void*)dst)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    int64_t x0 = p0.x < p1.x ? p0.x : p1.x;
    int64_t x1 = p0.x < p1.x ? p1.x : p0.x;
    int64_t y0 = p0.y < p1.y ? p0.y : p1.y;
    int64_t y1 = p0.y < p1.y ? p1.y : p0.y;

    if (x0 < 0 || y0 < 0 || x1 <= x0 || y1 <= y0 || x1 > (int64_t)src->width ||
        y1 > (int64_t)src->height)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    uint32_t w = (uint32_t)(x1 - x0);
    uint32_t h = (uint32_t)(y1 - y0);

    mitya_image_result_t res =
        mitya_image_create(dst, w, h, src->channels, src->type);
    if (res != MITYA_IMAGE_SUCCESS) return res;

    size_t px = mitya_sample_size(src->type) * src->channels;
    for (uint32_t r = 0; r < h; ++r) {
        memcpy(px_at_mut(dst, px, 0, r),
               px_at(src, px, (uint32_t)x0, (uint32_t)y0 + r), (size_t)w * px);
    }

    return MITYA_IMAGE_SUCCESS;
}

mitya_image_result_t mitya_image_rotate90(const mitya_image_t* src,
                                          mitya_image_t* dst,
                                          int quarter_turns) {
    if (!mitya_image_valid(src) || !dst || (const void*)src == (void*)dst)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    int k = ((quarter_turns % 4) + 4) % 4;
    uint32_t w = src->width, h = src->height;
    uint32_t dw = (k & 1) ? h : w, dh = (k & 1) ? w : h;

    mitya_image_result_t res =
        mitya_image_create(dst, dw, dh, src->channels, src->type);

    if (res != MITYA_IMAGE_SUCCESS) return res;

    size_t px = mitya_sample_size(src->type) * src->channels;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t dx, dy;

            switch (k) {
                case 1:
                    dx = h - 1 - y;
                    dy = x;
                    break;
                case 2:
                    dx = w - 1 - x;
                    dy = h - 1 - y;
                    break;
                case 3:
                    dx = y;
                    dy = w - 1 - x;
                    break;
                default:
                    dx = x;
                    dy = y;
                    break;
            }

            memcpy(px_at_mut(dst, px, dx, dy), px_at(src, px, x, y), px);
        }
    }

    return MITYA_IMAGE_SUCCESS;
}

static void sample_into(uint8_t* out, const mitya_image_t* s, double fx,
                        double fy, mitya_interp_t interp, const double* fill) {
    size_t ss = mitya_sample_size(s->type);
    size_t px = ss * s->channels;
    int inside = fx >= 0.0 && fy >= 0.0 && fx < (double)s->width &&
                 fy < (double)s->height;

    if (!inside) {
        for (uint8_t c = 0; c < s->channels; ++c)
            // & cycles b/w 0-3
            mitya_write_sample(out + c * ss, s->type, fill ? fill[c & 3] : 0.0);

        return;
    }

    if (interp == MITYA_INTERP_NEAREST) {
        memcpy(out, px_at(s, px, (uint32_t)fx, (uint32_t)fy), px);

        return;
    }

    double gx = fx - 0.5;
    double gy = fy - 0.5;

    double fx0 = floor(gx);
    double fy0 = floor(gy);

    double tx = gx - fx0;
    double ty = gy - fy0;

    int64_t xa = (int64_t)fx0;
    int64_t ya = (int64_t)fy0;

    int64_t xb = xa + 1;
    int64_t yb = ya + 1;

    int64_t mx = (int64_t)s->width - 1;
    int64_t my = (int64_t)s->height - 1;

    if (xa < 0) xa = 0;
    if (xb < 0) xb = 0;
    if (xa > mx) xa = mx;
    if (xb > mx) xb = mx;
    if (ya < 0) ya = 0;
    if (yb < 0) yb = 0;
    if (ya > my) ya = my;
    if (yb > my) yb = my;

    const uint8_t* p00 = px_at(s, px, (uint32_t)xa, (uint32_t)ya);
    const uint8_t* p10 = px_at(s, px, (uint32_t)xb, (uint32_t)ya);
    const uint8_t* p01 = px_at(s, px, (uint32_t)xa, (uint32_t)yb);
    const uint8_t* p11 = px_at(s, px, (uint32_t)xb, (uint32_t)yb);

    for (uint8_t c = 0; c < s->channels; ++c) {
        size_t o = c * ss;

        double top = mitya_read_sample(p00 + o, s->type) * (1 - tx) +
                     mitya_read_sample(p10 + o, s->type) * tx;
        double bot = mitya_read_sample(p01 + o, s->type) * (1 - tx) +
                     mitya_read_sample(p11 + o, s->type) * tx;

        mitya_write_sample(out + o, s->type, top * (1 - ty) + bot * ty);
    }
}

mitya_image_result_t mitya_image_rotate(const mitya_image_t* src,
                                        mitya_image_t* dst,
                                        const mitya_rotate_opts_t* opts) {
    if (!mitya_image_valid(src) || !dst || (const void*)src == (void*)dst ||
        !opts || !isfinite(opts->angle_deg))
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    double quarters = opts->angle_deg / 90.0;
    if (opts->expand && quarters == floor(quarters) && fabs(quarters) < 1e9)
        return mitya_image_rotate90(src, dst, (int)quarters);

    double th = opts->angle_deg * PI / 180.0;
    double c = cos(th);
    double s = sin(th);
    double sw = src->width;
    double sh = src->height;

    uint32_t dw = src->width;
    uint32_t dh = src->height;
    if (opts->expand) {
        double fw = fabs(sw * c) + fabs(sh * s);
        double fh = fabs(sw * s) + fabs(sh * c);

        if (fw > 4294967295.0 || fh > 4294967295.0)
            return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

        dw = (uint32_t)ceil(fw - 1e-9);
        dh = (uint32_t)ceil(fh - 1e-9);

        if (dw == 0) dw = 1;
        if (dh == 0) dh = 1;
    }

    mitya_image_result_t res =
        mitya_image_create(dst, dw, dh, src->channels, src->type);
    if (res != MITYA_IMAGE_SUCCESS) return res;

    size_t px = mitya_sample_size(src->type) * src->channels;
    double scx = sw / 2.0;
    double scy = sh / 2.0;
    double dcx = dw / 2.0;
    double dcy = dh / 2.0;

    for (uint32_t y = 0; y < dh; ++y) {
        for (uint32_t x = 0; x < dw; ++x) {
            double ux = (x + 0.5) - dcx;
            double uy = (y + 0.5) - dcy;
            double fx = ux * c + uy * s + scx;
            double fy = -ux * s + uy * c + scy;

            sample_into(px_at_mut(dst, px, x, y), src, fx, fy, opts->interp,
                        opts->fill);
        }
    }

    return MITYA_IMAGE_SUCCESS;
}

mitya_image_result_t mitya_image_resize(const mitya_image_t* src,
                                        mitya_image_t* dst, uint32_t w,
                                        uint32_t h, mitya_interp_t interp) {
    if (!mitya_image_valid(src) || !dst || (const void*)src == (void*)dst ||
        w == 0 || h == 0)
        return MITYA_IMAGE_ERROR_INVALID_ARGUMENT;

    mitya_image_result_t res =
        mitya_image_create(dst, w, h, src->channels, src->type);
    if (res != MITYA_IMAGE_SUCCESS) return res;

    size_t px = mitya_sample_size(src->type) * src->channels;
    double kx = (double)src->width / w, ky = (double)src->height / h;

    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            sample_into(px_at_mut(dst, px, x, y), src, (x + 0.5) * kx,
                        (y + 0.5) * ky, interp, NULL);

    return MITYA_IMAGE_SUCCESS;
}