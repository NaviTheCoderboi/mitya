#ifndef MITYA_IMAGE_HPP
#define MITYA_IMAGE_HPP

#if __cplusplus < 202300L
#error "C++23 or later is required"
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mitya/image.h"
#include "mitya/ops.h"

namespace mitya {

enum class Result : int {
    Success = MITYA_IMAGE_SUCCESS,
    InvalidArgument = MITYA_IMAGE_ERROR_INVALID_ARGUMENT,
    FileNotFound = MITYA_IMAGE_ERROR_FILE_NOT_FOUND,
    Io = MITYA_IMAGE_ERROR_IO,
    DecodeFailed = MITYA_IMAGE_ERROR_DECODE_FAILED,
    EncodeFailed = MITYA_IMAGE_ERROR_ENCODE_FAILED,
    OutOfMemory = MITYA_IMAGE_ERROR_OUT_OF_MEMORY,
    UnsupportedFormat = MITYA_IMAGE_ERROR_UNSUPPORTED_FORMAT
};

enum class PixelType : int {
    U8 = MITYA_IMAGE_TYPE_U8,
    U16 = MITYA_IMAGE_TYPE_U16,
    U32 = MITYA_IMAGE_TYPE_U32,
    F32 = MITYA_IMAGE_TYPE_F32,
    F64 = MITYA_IMAGE_TYPE_F64
};

struct Point {
    std::int32_t x, y;
};

enum class Interpolation : int {
    Nearest = MITYA_INTERP_NEAREST,
    Bilinear = MITYA_INTERP_BILINEAR
};

struct RotateOptions {
    double angle_deg;
    Interpolation interp;
    bool expand;
    double fill[4];
};

enum class Format : int {
    Unknown = MITYA_IMAGE_FORMAT_UNKNOWN,
    Png = MITYA_IMAGE_FORMAT_PNG,
    Pnm = MITYA_IMAGE_FORMAT_PNM,

#ifdef MITYA_USER_CODECS_HEADER
#define MITYA_X_FORMAT_CLASS(name, symbol) name = MITYA_IMAGE_FORMAT_##name,
    MITYA_USER_CODECS(MITYA_X_FORMAT_CLASS)
#undef MITYA_X_FORMAT_CLASS
#endif

};

template <class T>
using Expected = std::expected<T, Result>;
using Status = std::expected<void, Result>;

inline const char* to_string(Result r) noexcept {
    switch (r) {
        case Result::Success:
            return "success";
        case Result::InvalidArgument:
            return "invalid argument";
        case Result::FileNotFound:
            return "file not found";
        case Result::Io:
            return "I/O error";
        case Result::DecodeFailed:
            return "decode failed";
        case Result::EncodeFailed:
            return "encode failed";
        case Result::OutOfMemory:
            return "out of memory";
        case Result::UnsupportedFormat:
            return "unsupported format";
    }

    return "unknown error";
}

namespace detail {

inline mitya_image_pixel_type_t to_c(PixelType t) noexcept {
    return static_cast<mitya_image_pixel_type_t>(t);
}

inline mitya_image_format_t to_c(Format f) noexcept {
    return static_cast<mitya_image_format_t>(f);
}

inline mitya_interp_t to_c(Interpolation i) noexcept {
    return static_cast<mitya_interp_t>(i);
}

inline mitya_point_t to_c(Point p) noexcept { return mitya_point_t{p.x, p.y}; }

inline mitya_rotate_opts_t to_c(const RotateOptions& opts) noexcept {
    return mitya_rotate_opts_t{
        opts.angle_deg,
        to_c(opts.interp),
        opts.expand,
        {opts.fill[0], opts.fill[1], opts.fill[2], opts.fill[3]}};
}

inline Result from_c(mitya_image_result_t r) noexcept {
    return static_cast<Result>(r);
}

inline bool ieq(std::string_view s, std::string_view lower) noexcept {
    if (s.size() != lower.size()) return false;

    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');

        if (c != lower[i]) return false;
    }

    return true;
}

inline std::unexpected<Result> fail(Result r) noexcept {
    return std::unexpected<Result>(r);
}

inline Status to_status(mitya_image_result_t c) noexcept {
    Result r = from_c(c);

    if (r == Result::Success) return Status();

    return fail(r);
}

struct BufferGuard {
    void* ptr = nullptr;

    BufferGuard() = default;
    BufferGuard(const BufferGuard&) = delete;
    BufferGuard& operator=(const BufferGuard&) = delete;

    ~BufferGuard() { mitya_buffer_free(&ptr); }
};

}  // namespace detail

inline Format format_from_path(std::string_view path) noexcept {
    std::size_t dot = path.find_last_of('.');

    if (dot == std::string_view::npos) return Format::Unknown;

    std::string_view ext = path.substr(dot + 1);

    if (detail::ieq(ext, "png")) return Format::Png;
    if (detail::ieq(ext, "ppm") || detail::ieq(ext, "pgm") ||
        detail::ieq(ext, "pnm"))
        return Format::Pnm;

    return Format::Unknown;
}

class Image {
   public:
    Image() noexcept { std::memset(&img_, 0, sizeof img_); }
    ~Image() { mitya_image_destroy(&img_); }

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other) noexcept : Image() { swap(other); }
    Image& operator=(Image&& other) noexcept {
        Image tmp(std::move(other));

        swap(tmp);

        return *this;
    }

    void swap(Image& other) noexcept { std::swap(img_, other.img_); }
    friend void swap(Image& a, Image& b) noexcept { a.swap(b); }

    [[nodiscard]] Expected<Image> crop(Point p0, Point p1) const noexcept {
        Image out;
        Result r = detail::from_c(mitya_image_crop(
            &img_, &out.img_, detail::to_c(p0), detail::to_c(p1)));

        if (r != Result::Success) return detail::fail(r);

        return Expected<Image>(std::move(out));
    };

    [[nodiscard]] Expected<Image> rotate90(int quarter_turns) const noexcept {
        Image out;
        Result r = detail::from_c(
            mitya_image_rotate90(&img_, &out.img_, quarter_turns));

        if (r != Result::Success) return detail::fail(r);

        return Expected<Image>(std::move(out));
    }

    [[nodiscard]] Expected<Image> rotate(
        const RotateOptions& opts) const noexcept {
        mitya_rotate_opts_t options = detail::to_c(opts);

        Image out;
        Result r =
            detail::from_c(mitya_image_rotate(&img_, &out.img_, &options));

        if (r != Result::Success) return detail::fail(r);

        return Expected<Image>(std::move(out));
    }

    [[nodiscard]] Expected<Image> resize(std::uint32_t w, std::uint32_t h,
                                         Interpolation interp) const noexcept {
        Image out;
        Result r = detail::from_c(
            mitya_image_resize(&img_, &out.img_, w, h, detail::to_c(interp)));

        if (r != Result::Success) return detail::fail(r);

        return Expected<Image>(std::move(out));
    }

    [[nodiscard]] Status invert() noexcept {
        return detail::to_status(mitya_image_invert(&img_));
    }

    [[nodiscard]] static Expected<Image> create(
        std::uint32_t width, std::uint32_t height, std::uint8_t channels,
        PixelType type = PixelType::U8) noexcept {
        Image img;

        Result r = detail::from_c(mitya_image_create(
            &img.img_, width, height, channels, detail::to_c(type)));

        if (r != Result::Success) return detail::fail(r);

        return Expected<Image>(std::move(img));
    }

    [[nodiscard]] Expected<Image> clone() const noexcept {
        if (empty()) return Expected<Image>(Image());

        Expected<Image> copy = create(width(), height(), channels(), type());
        if (!copy) return copy;

        std::size_t n = std::min(copy->img_.row_stride, img_.row_stride);
        for (std::uint32_t y = 0; y < height(); ++y)
            std::memcpy(copy->row<std::uint8_t>(y), row<std::uint8_t>(y), n);

        return copy;
    }

    [[nodiscard]] static Expected<Image> load_from_memory(
        const void* data, std::size_t size, Format format = Format::Unknown,
        PixelType type = PixelType::U8) noexcept {
        Image img;

        Result r = detail::from_c(mitya_image_load_from_buffer(
            &img.img_, data, size, detail::to_c(format), detail::to_c(type)));

        if (r != Result::Success) return detail::fail(r);

        return Expected<Image>(std::move(img));
    }

    [[nodiscard]] static Expected<Image> load_from_memory(
        const std::vector<std::uint8_t>& bytes, Format format = Format::Unknown,
        PixelType type = PixelType::U8) noexcept {
        return load_from_memory(bytes.data(), bytes.size(), format, type);
    }

    [[nodiscard]] static Expected<Image> load(
        const std::string& path, Format format = Format::Unknown,
        PixelType type = PixelType::U8) noexcept {
        Image img;

        Result r = detail::from_c(mitya_image_load_from_file(
            &img.img_, path.c_str(), detail::to_c(format), detail::to_c(type)));

        if (r != Result::Success) return detail::fail(r);

        return Expected<Image>(std::move(img));
    }

    [[nodiscard]] Status save(const std::string& path, Format format,
                              PixelType type) const noexcept {
        return detail::to_status(mitya_image_save_to_file(
            &img_, path.c_str(), detail::to_c(format), detail::to_c(type)));
    }

    [[nodiscard]] Status save(const std::string& path,
                              Format format) const noexcept {
        return save(path, format, type());
    }

    [[nodiscard]] Status save(const std::string& path) const noexcept {
        return save(path, format_from_path(path), type());
    }

    [[nodiscard]] Expected<std::vector<std::uint8_t>> encode(
        Format format, PixelType type) const noexcept {
        detail::BufferGuard buf;
        std::size_t size = 0;

        Result r = detail::from_c(mitya_image_save_to_buffer(
            &img_, &buf.ptr, &size, detail::to_c(format), detail::to_c(type)));

        if (r != Result::Success) return detail::fail(r);

        const std::uint8_t* p = static_cast<const std::uint8_t*>(buf.ptr);

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try {
            return Expected<std::vector<std::uint8_t>>(std::in_place, p,
                                                       p + size);
        } catch (...) {
            return detail::fail(Result::OutOfMemory);
        }
#else
        return Expected<std::vector<std::uint8_t>>(std::in_place, p, p + size);
#endif
    }

    [[nodiscard]] Expected<std::vector<std::uint8_t>> encode(
        Format format) const noexcept {
        return encode(format, type());
    }

    bool empty() const noexcept { return img_.pixels == nullptr; }
    explicit operator bool() const noexcept { return !empty(); }

    std::uint32_t width() const noexcept { return mitya_image_width(&img_); }
    std::uint32_t height() const noexcept { return mitya_image_height(&img_); }
    std::uint8_t channels() const noexcept {
        return mitya_image_channels(&img_);
    }
    PixelType type() const noexcept {
        return static_cast<PixelType>(mitya_image_pixel_type(&img_));
    }

    std::size_t row_stride() const noexcept { return img_.row_stride; }
    std::size_t size_bytes() const noexcept { return img_.data_size; }

    std::size_t sample_size() const noexcept {
        switch (type()) {
            case PixelType::U8:
                return 1;
            case PixelType::U16:
                return 2;
            case PixelType::U32:
                return 4;
            case PixelType::F32:
                return 4;
            case PixelType::F64:
                return 8;
        }

        return 0;
    }

    void* data() noexcept { return img_.pixels; }
    const void* data() const noexcept { return img_.pixels; }

    template <class T>
    T* row(std::uint32_t y) noexcept {
        return reinterpret_cast<T*>(static_cast<std::uint8_t*>(img_.pixels) +
                                    static_cast<std::size_t>(y) *
                                        img_.row_stride);
    }

    template <class T>
    const T* row(std::uint32_t y) const noexcept {
        return reinterpret_cast<const T*>(
            static_cast<const std::uint8_t*>(img_.pixels) +
            static_cast<std::size_t>(y) * img_.row_stride);
    }

    template <class T>
    T* try_at(std::uint32_t x, std::uint32_t y, std::uint32_t c = 0) noexcept {
        if (!access_ok(sizeof(T), x, y, c)) return nullptr;

        return row<T>(y) + static_cast<std::size_t>(x) * channels() + c;
    }

    template <class T>
    const T* try_at(std::uint32_t x, std::uint32_t y,
                    std::uint32_t c = 0) const noexcept {
        if (!access_ok(sizeof(T), x, y, c)) return nullptr;

        return row<T>(y) + static_cast<std::size_t>(x) * channels() + c;
    }

    template <class T>
    T& at(std::uint32_t x, std::uint32_t y, std::uint32_t c = 0) noexcept {
        assert(access_ok(sizeof(T), x, y, c));

        return row<T>(y)[static_cast<std::size_t>(x) * channels() + c];
    }

    template <class T>
    const T& at(std::uint32_t x, std::uint32_t y,
                std::uint32_t c = 0) const noexcept {
        assert(access_ok(sizeof(T), x, y, c));

        return row<T>(y)[static_cast<std::size_t>(x) * channels() + c];
    }

    mitya_image_t* native() noexcept { return &img_; }
    const mitya_image_t* native() const noexcept { return &img_; }

    mitya_image_t release() noexcept {
        mitya_image_t out = img_;

        std::memset(&img_, 0, sizeof img_);

        return out;
    }

    static Image adopt(mitya_image_t& c_image) noexcept {
        Image img;

        img.img_ = c_image;
        std::memset(&c_image, 0, sizeof c_image);

        return img;
    }

   private:
    bool access_ok(std::size_t sample_bytes, std::uint32_t x, std::uint32_t y,
                   std::uint32_t c) const noexcept {
        return !empty() && x < img_.width && y < img_.height &&
               c < img_.channels && sample_bytes == sample_size();
    }

    mitya_image_t img_;
};

}  // namespace mitya

#endif  // MITYA_IMAGE_HPP