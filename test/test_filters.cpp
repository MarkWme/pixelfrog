#include "filters.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using pixelfrog::FilterParams;
using pixelfrog::ImageView;

namespace {

ImageView rgb2x1(std::uint8_t r0, std::uint8_t g0, std::uint8_t b0, std::uint8_t r1,
                 std::uint8_t g1, std::uint8_t b1) {
    ImageView im;
    im.width = 2;
    im.height = 1;
    im.channels = 3;
    im.pixels = {r0, g0, b0, r1, g1, b1};
    return im;
}

} // namespace

TEST_CASE("greyscale uses BT.601 coefficients", "[filters]") {
    auto im = rgb2x1(255, 0, 0, 0, 255, 0);
    REQUIRE(pixelfrog::apply_greyscale(im));
    const int y_r = static_cast<int>(
        std::lround(0.299 * 255.0 + 0.587 * 0.0 + 0.114 * 0.0));
    const int y_g = static_cast<int>(
        std::lround(0.299 * 0.0 + 0.587 * 255.0 + 0.114 * 0.0));
    REQUIRE(static_cast<int>(im.pixels[0]) == y_r);
    REQUIRE(static_cast<int>(im.pixels[1]) == y_r);
    REQUIRE(static_cast<int>(im.pixels[2]) == y_r);
    REQUIRE(static_cast<int>(im.pixels[3]) == y_g);
    REQUIRE(static_cast<int>(im.pixels[4]) == y_g);
    REQUIRE(static_cast<int>(im.pixels[5]) == y_g);
}

TEST_CASE("brighten clamps high", "[filters]") {
    auto im = rgb2x1(250, 250, 250, 10, 10, 10);
    FilterParams p;
    p.brighten_delta = 30;
    REQUIRE(pixelfrog::apply_brighten(im, p));
    REQUIRE(static_cast<int>(im.pixels[0]) == 255);
    REQUIRE(static_cast<int>(im.pixels[3]) == 40);
}

TEST_CASE("darken clamps low", "[filters]") {
    auto im = rgb2x1(5, 5, 5, 100, 100, 100);
    FilterParams p;
    p.darken_delta = 30;
    REQUIRE(pixelfrog::apply_darken(im, p));
    REQUIRE(static_cast<int>(im.pixels[0]) == 0);
    REQUIRE(static_cast<int>(im.pixels[3]) == 70);
}

TEST_CASE("blur preserves uniform region", "[filters]") {
    ImageView im;
    im.width = 3;
    im.height = 3;
    im.channels = 3;
    im.pixels.assign(27, 200);
    FilterParams p;
    p.blur_sigma = 1.0;
    REQUIRE(pixelfrog::apply_blur(im, p));
    for (std::uint8_t v : im.pixels) {
        REQUIRE(static_cast<int>(v) == 200);
    }
}

TEST_CASE("edges on flat interior is zero", "[filters]") {
    ImageView im;
    im.width = 5;
    im.height = 5;
    im.channels = 3;
    im.pixels.assign(75, 42);
    REQUIRE(pixelfrog::apply_edges(im));
    const int cx = 2;
    const int cy = 2;
    const int o = (cy * im.width + cx) * 3;
    REQUIRE(static_cast<int>(im.pixels[static_cast<std::size_t>(o)]) == 0);
}

TEST_CASE("apply_filter_by_name rejects unknown", "[filters]") {
    ImageView im = rgb2x1(1, 2, 3, 4, 5, 6);
    FilterParams p;
    REQUIRE_FALSE(pixelfrog::apply_filter_by_name(im, "nope", p));
}

TEST_CASE("apply_filter_by_name greyscale", "[filters]") {
    ImageView im = rgb2x1(255, 0, 0, 0, 0, 255);
    FilterParams p;
    REQUIRE(pixelfrog::apply_filter_by_name(im, "greyscale", p));
    REQUIRE(im.pixels[0] == im.pixels[1]);
}
