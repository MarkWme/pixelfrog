#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pixelfrog {

struct FilterParams {
    int blur_radius = 1;
    double blur_sigma = 1.0;
    int brighten_delta = 30;
    int darken_delta = 30;
};

struct ImageView {
    int width = 0;
    int height = 0;
    int channels = 3; // RGB
    std::vector<std::uint8_t> pixels;
};

bool apply_greyscale(ImageView &img);
bool apply_blur(ImageView &img, const FilterParams &params);
bool apply_edges(ImageView &img);
bool apply_brighten(ImageView &img, const FilterParams &params);
bool apply_darken(ImageView &img, const FilterParams &params);

/// Returns false if name is unknown.
bool apply_filter_by_name(ImageView &img, const std::string &name,
                          const FilterParams &params);

} // namespace pixelfrog
