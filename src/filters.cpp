#include "filters.h"

#include <algorithm>
#include <cmath>
namespace pixelfrog {

namespace {

inline std::uint8_t clamp_byte(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return static_cast<std::uint8_t>(v);
}

void gaussian_kernel_3x3(double sigma, double out[9]) {
    const int k = 1;
    double sum = 0.0;
    int i = 0;
    for (int y = -k; y <= k; ++y) {
        for (int x = -k; x <= k; ++x) {
            double g =
                std::exp(-(x * x + y * y) / (2.0 * sigma * sigma));
            out[i++] = g;
            sum += g;
        }
    }
    for (int j = 0; j < 9; ++j) {
        out[j] /= sum;
    }
}

} // namespace

bool apply_greyscale(ImageView &img) {
    if (img.channels != 3 || img.pixels.empty()) {
        return false;
    }
    const std::size_t n = img.pixels.size();
    for (std::size_t i = 0; i < n; i += 3) {
        const int r = img.pixels[i];
        const int g = img.pixels[i + 1];
        const int b = img.pixels[i + 2];
        const int y = static_cast<int>(
            std::lround(0.299 * r + 0.587 * g + 0.114 * b));
        const std::uint8_t gv = clamp_byte(y);
        img.pixels[i] = gv;
        img.pixels[i + 1] = gv;
        img.pixels[i + 2] = gv;
    }
    return true;
}

bool apply_blur(ImageView &img, const FilterParams &params) {
    if (img.channels != 3 || img.pixels.empty()) {
        return false;
    }
    // Spec: 3×3 Gaussian; radius in config is reserved for future kernels.
    (void)params.blur_radius;
    double kernel[9];
    const double sigma = std::max(params.blur_sigma, 0.1);
    gaussian_kernel_3x3(sigma, kernel);

    std::vector<std::uint8_t> src = img.pixels;
    const int w = img.width;
    const int h = img.height;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double acc[3] = {0, 0, 0};
            int ki = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    const int sx = std::clamp(x + kx, 0, w - 1);
                    const int sy = std::clamp(y + ky, 0, h - 1);
                    const int idx = (sy * w + sx) * 3;
                    const double wt = kernel[ki++];
                    acc[0] += src[idx] * wt;
                    acc[1] += src[idx + 1] * wt;
                    acc[2] += src[idx + 2] * wt;
                }
            }
            const int o = (y * w + x) * 3;
            img.pixels[o] = clamp_byte(static_cast<int>(std::lround(acc[0])));
            img.pixels[o + 1] =
                clamp_byte(static_cast<int>(std::lround(acc[1])));
            img.pixels[o + 2] =
                clamp_byte(static_cast<int>(std::lround(acc[2])));
        }
    }
    return true;
}

bool apply_edges(ImageView &img) {
    if (img.channels != 3 || img.pixels.empty()) {
        return false;
    }
    apply_greyscale(img);

    static const int gx[9] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
    static const int gy[9] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};

    std::vector<std::uint8_t> src = img.pixels;
    const int w = img.width;
    const int h = img.height;
    std::vector<std::uint8_t> out(img.pixels.size());

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double sx = 0, sy = 0;
            for (int k = 0; k < 9; ++k) {
                const int ox = k % 3 - 1;
                const int oy = k / 3 - 1;
                const int px = std::clamp(x + ox, 0, w - 1);
                const int py = std::clamp(y + oy, 0, h - 1);
                const int v = src[(py * w + px) * 3];
                sx += static_cast<double>(v) * gx[k];
                sy += static_cast<double>(v) * gy[k];
            }
            const double mag = std::sqrt(sx * sx + sy * sy);
            const std::uint8_t e = clamp_byte(static_cast<int>(std::lround(mag)));
            const int o = (y * w + x) * 3;
            out[o] = e;
            out[o + 1] = e;
            out[o + 2] = e;
        }
    }
    img.pixels = std::move(out);
    return true;
}

bool apply_brighten(ImageView &img, const FilterParams &params) {
    if (img.channels != 3 || img.pixels.empty()) {
        return false;
    }
    const int d = params.brighten_delta;
    for (std::size_t i = 0; i < img.pixels.size(); ++i) {
        img.pixels[i] = clamp_byte(static_cast<int>(img.pixels[i]) + d);
    }
    return true;
}

bool apply_darken(ImageView &img, const FilterParams &params) {
    if (img.channels != 3 || img.pixels.empty()) {
        return false;
    }
    const int d = params.darken_delta;
    for (std::size_t i = 0; i < img.pixels.size(); ++i) {
        img.pixels[i] = clamp_byte(static_cast<int>(img.pixels[i]) - d);
    }
    return true;
}

bool apply_filter_by_name(ImageView &img, const std::string &name,
                          const FilterParams &params) {
    if (name == "greyscale") {
        return apply_greyscale(img);
    }
    if (name == "blur") {
        return apply_blur(img, params);
    }
    if (name == "edges") {
        return apply_edges(img);
    }
    if (name == "brighten") {
        return apply_brighten(img, params);
    }
    if (name == "darken") {
        return apply_darken(img, params);
    }
    return false;
}

} // namespace pixelfrog
