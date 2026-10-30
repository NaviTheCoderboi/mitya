#include <iostream>

#include "mitya/image.hpp"

int main(void) {
    constexpr std::uint32_t size = 512;
    constexpr std::uint32_t squareSize = size / 8;

    auto result = mitya::Image::create(size, size, 3);

    if (!result) {
        std::cerr << mitya::to_string(result.error()) << '\n';

        return 1;
    }

    mitya::Image image = std::move(*result);

    constexpr std::uint8_t light[] = {238, 238, 210};
    constexpr std::uint8_t dark[] = {118, 150, 86};

    for (std::uint32_t y = 0; y < size; ++y) {
        auto* row = image.row<std::uint8_t>(y);

        for (std::uint32_t x = 0; x < size; ++x) {
            const auto* color =
                ((x / squareSize + y / squareSize) % 2 == 0) ? light : dark;

            const std::uint32_t pixel = x * 3;

            row[pixel + 0] = color[0];
            row[pixel + 1] = color[1];
            row[pixel + 2] = color[2];
        }
    }

    if (auto res = image.save("original.png"); !res) {
        std::cerr << mitya::to_string(res.error()) << '\n';

        return 1;
    }

    // resize to less than original size
    auto result2 =
        image.resize(size / 2, size / 2, mitya::Interpolation::Bilinear);

    if (!result2) {
        std::cerr << mitya::to_string(result2.error()) << '\n';

        return 1;
    }

    if (auto res = result2->save("resized.png"); !res) {
        std::cerr << mitya::to_string(res.error()) << '\n';

        return 1;
    }

    // resize to more than original size
    auto result3 =
        image.resize(size * 4, size * 4, mitya::Interpolation::Bilinear);

    if (!result3) {
        std::cerr << mitya::to_string(result3.error()) << '\n';
        return 1;
    }

    if (auto res = result3->save("resized2.png"); !res) {
        std::cerr << mitya::to_string(res.error()) << '\n';
        return 1;
    }

    auto img2 = mitya::Image::load("assets/logo.png");

    if (!img2) {
        std::cerr << mitya::to_string(img2.error()) << '\n';
        return 1;
    }

    auto result4 = img2->resize(img2->width() * 4, img2->height() * 4,
                                mitya::Interpolation::Bilinear);

    if (!result4) {
        std::cerr << mitya::to_string(result4.error()) << '\n';
        return 1;
    }

    if (auto res = result4->save("resized3.png"); !res) {
        std::cerr << mitya::to_string(res.error()) << '\n';
        return 1;
    }

    return 0;
}