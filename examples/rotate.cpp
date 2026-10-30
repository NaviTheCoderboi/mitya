#include <iostream>

#include "mitya/image.hpp"

int main(void) {
    auto result = mitya::Image::load("assets/logo.ppm");

    if (!result) {
        std::cerr << mitya::to_string(result.error()) << '\n';

        return 1;
    }

    mitya::Image image = std::move(*result);

    // auto rotated = image.rotate90(1);

    // if (!rotated) {
    //     std::cerr << mitya::to_string(rotated.error()) << '\n';
    //     return 1;
    // }

    // if (auto res = rotated->save("rotated90.png"); !res) {
    //     std::cerr << mitya::to_string(res.error()) << '\n';
    //     return 1;
    // }

    auto rotated = image.rotate({.angle_deg = 45.0f,
                                 .interp = mitya::Interpolation::Bilinear,
                                 .expand = true,
                                 .fill = {0, 0, 0, 0}});

    if (!rotated) {
        std::cerr << mitya::to_string(rotated.error()) << '\n';
        return 1;
    }

    if (auto res = rotated->save("rotated45.ppm"); !res) {
        std::cerr << mitya::to_string(res.error()) << '\n';
        return 1;
    }

    return 0;
}