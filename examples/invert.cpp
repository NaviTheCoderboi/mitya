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

    if (auto saveResult = image.save("chessboard.png", mitya::Format::Png);
        !saveResult) {
        std::cerr << mitya::to_string(saveResult.error()) << '\n';

        return 1;
    }

    if (auto invertResult = image.invert(); !invertResult) {
        std::cerr << mitya::to_string(invertResult.error()) << '\n';

        return 1;
    };

    if (auto saveResult = image.save("inverted.png", mitya::Format::Png);
        !saveResult) {
        std::cerr << mitya::to_string(saveResult.error()) << '\n';

        return 1;
    }

    return 0;
}