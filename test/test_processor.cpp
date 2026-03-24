#include "processor.h"

#include "logger.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("default_config_json has expected keys", "[processor]") {
    const auto j = pixelfrog::default_config_json();
    REQUIRE(j.contains("blur"));
    REQUIRE(j.contains("brighten"));
    REQUIRE(j.contains("darken"));
    REQUIRE(j.contains("output"));
    REQUIRE(j["blur"]["radius"].get<int>() == 1);
}

TEST_CASE("json_merge_inplace deep merges objects", "[processor]") {
    auto base = pixelfrog::default_config_json();
    nlohmann::json over;
    over["brighten"] = nlohmann::json::object();
    over["brighten"]["delta"] = 99;
    over["blur"]["sigma"] = 2.5;
    pixelfrog::json_merge_inplace(base, over);
    REQUIRE(base["brighten"]["delta"].get<int>() == 99);
    REQUIRE(base["blur"]["sigma"].get<double>() == 2.5);
    REQUIRE(base["blur"]["radius"].get<int>() == 1);
}

TEST_CASE("filter_params_from_json reads deltas and blur", "[processor]") {
    nlohmann::json j;
    j["blur"] = {{"radius", 1}, {"sigma", 1.7}};
    j["brighten"] = {{"delta", 11}};
    j["darken"] = {{"delta", 22}};
    const auto p = pixelfrog::filter_params_from_json(j);
    REQUIRE(p.blur_sigma == 1.7);
    REQUIRE(p.brighten_delta == 11);
    REQUIRE(p.darken_delta == 22);
}

TEST_CASE("run_processor fails on missing input", "[processor]") {
    pixelfrog::init_logger({.verbose = false});
    pixelfrog::ProcessOptions opt;
    opt.input_path = "/no/such/file/pixelfrog_miss.jpg";
    opt.output_path = "/tmp/out.jpg";
    opt.filters = {"greyscale"};
    REQUIRE(pixelfrog::run_processor(opt) != 0);
    pixelfrog::shutdown_logger();
}

TEST_CASE("run_processor processes sample png", "[processor]") {
    const fs::path root =
        fs::path(__FILE__).parent_path().parent_path() / "test_images";
    const fs::path in = root / "sample.png";
    REQUIRE(fs::exists(in));
    const fs::path out =
        fs::temp_directory_path() /
        ("pixelfrog_test_out_" + std::to_string(std::rand()) + ".png");
    pixelfrog::init_logger({.verbose = false});
    pixelfrog::ProcessOptions opt;
    opt.input_path = in.string();
    opt.output_path = out.string();
    opt.filters = {"greyscale"};
    REQUIRE(pixelfrog::run_processor(opt) == 0);
    REQUIRE(fs::file_size(out) > 0);
    fs::remove(out);
    pixelfrog::shutdown_logger();
}

TEST_CASE("run_processor honours jpeg_quality cli", "[processor]") {
    const fs::path root =
        fs::path(__FILE__).parent_path().parent_path() / "test_images";
    const fs::path in = root / "sample.jpg";
    REQUIRE(fs::exists(in));
    const fs::path out =
        fs::temp_directory_path() /
        ("pixelfrog_q_" + std::to_string(std::rand()) + ".jpg");
    pixelfrog::init_logger({.verbose = false});
    pixelfrog::ProcessOptions opt;
    opt.input_path = in.string();
    opt.output_path = out.string();
    opt.filters = {"blur"};
    opt.jpeg_quality = 40;
    REQUIRE(pixelfrog::run_processor(opt) == 0);
    REQUIRE(fs::file_size(out) > 0);
    fs::remove(out);
    pixelfrog::shutdown_logger();
}
