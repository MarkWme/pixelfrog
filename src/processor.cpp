#include "processor.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>

namespace pixelfrog {

nlohmann::json default_config_json() {
    return nlohmann::json::parse(R"({
    "blur": { "radius": 1, "sigma": 1.0 },
    "brighten": { "delta": 30 },
    "darken": { "delta": 30 },
    "output": { "jpeg_quality": 85 }
})");
}

void json_merge_inplace(nlohmann::json &base, const nlohmann::json &overrides) {
    if (!overrides.is_object()) {
        base = overrides;
        return;
    }
    if (!base.is_object()) {
        base = overrides;
        return;
    }
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        const auto &key = it.key();
        const auto &val = it.value();
        if (base.contains(key) && base[key].is_object() && val.is_object()) {
            json_merge_inplace(base[key], val);
        } else {
            base[key] = val;
        }
    }
}

FilterParams filter_params_from_json(const nlohmann::json &cfg) {
    FilterParams p;
    if (cfg.contains("blur")) {
        const auto &b = cfg["blur"];
        if (b.contains("radius")) {
            p.blur_radius = b["radius"].get<int>();
        }
        if (b.contains("sigma")) {
            p.blur_sigma = b["sigma"].get<double>();
        }
    }
    if (cfg.contains("brighten") && cfg["brighten"].contains("delta")) {
        p.brighten_delta = cfg["brighten"]["delta"].get<int>();
    }
    if (cfg.contains("darken") && cfg["darken"].contains("delta")) {
        p.darken_delta = cfg["darken"]["delta"].get<int>();
    }
    return p;
}

namespace {

inline std::uint8_t clamp_byte(int v) {
    return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
}

std::string extension_lower(const std::string &path) {
    const auto pos = path.rfind('.');
    if (pos == std::string::npos) {
        return {};
    }
    std::string ext = path.substr(pos);
    for (char &c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

bool write_image(const std::string &path, const ImageView &img, int jpeg_quality) {
    const std::string ext = extension_lower(path);
    const int w = img.width;
    const int h = img.height;
    const int comp = img.channels;
    const int stride = w * comp;

    if (ext == ".png" || ext == ".PNG") {
        const int ok =
            stbi_write_png(path.c_str(), w, h, comp, img.pixels.data(), stride);
        return ok != 0;
    }
    // Default / jpg / jpeg
    const int q = std::clamp(jpeg_quality, 1, 100);
    const int ok = stbi_write_jpg(path.c_str(), w, h, comp, img.pixels.data(), q);
    return ok != 0;
}

} // namespace

int run_processor(const ProcessOptions &opt) {
    auto t0 = std::chrono::steady_clock::now();

    nlohmann::json cfg = default_config_json();
    if (!opt.config_path.empty()) {
        std::ifstream f(opt.config_path);
        if (!f) {
            spdlog::error("Cannot open config file: {}", opt.config_path);
            return 2;
        }
        nlohmann::json loaded;
        f >> loaded;
        json_merge_inplace(cfg, loaded);
    }

    int quality = 85;
    if (cfg.contains("output") && cfg["output"].contains("jpeg_quality")) {
        quality = cfg["output"]["jpeg_quality"].get<int>();
    }
    if (opt.jpeg_quality.has_value()) {
        quality = *opt.jpeg_quality;
    }
    quality = std::clamp(quality, 1, 100);

    int w = 0;
    int h = 0;
    int ch = 0;
    unsigned char *data = stbi_load(opt.input_path.c_str(), &w, &h, &ch, 0);
    if (!data) {
        spdlog::error("Failed to load image: {}", opt.input_path);
        return 3;
    }

    ImageView img;
    img.width = w;
    img.height = h;
    img.channels = ch;
    img.pixels.assign(data, data + static_cast<std::size_t>(w * h * ch));
    stbi_image_free(data);

    if (ch == 1) {
        // Expand greyscale to RGB for a single pipeline
        std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w * h * 3));
        for (int i = 0; i < w * h; ++i) {
            const std::uint8_t g = img.pixels[static_cast<std::size_t>(i)];
            rgb[static_cast<std::size_t>(i * 3)] = g;
            rgb[static_cast<std::size_t>(i * 3 + 1)] = g;
            rgb[static_cast<std::size_t>(i * 3 + 2)] = g;
        }
        img.channels = 3;
        img.pixels = std::move(rgb);
    } else if (ch == 4) {
        // Drop alpha: blend on white (simple premultiply inverse for demo)
        std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w * h * 3));
        for (int i = 0; i < w * h; ++i) {
            const std::size_t p = static_cast<std::size_t>(i * 4);
            const int r = img.pixels[p];
            const int g = img.pixels[p + 1];
            const int b = img.pixels[p + 2];
            const int a = img.pixels[p + 3];
            rgb[static_cast<std::size_t>(i * 3)] = clamp_byte(
                (r * a + 255 * (255 - a)) / 255);
            rgb[static_cast<std::size_t>(i * 3 + 1)] = clamp_byte(
                (g * a + 255 * (255 - a)) / 255);
            rgb[static_cast<std::size_t>(i * 3 + 2)] = clamp_byte(
                (b * a + 255 * (255 - a)) / 255);
        }
        img.channels = 3;
        img.pixels = std::move(rgb);
    } else if (ch != 3) {
        spdlog::error("Unsupported channel count: {}", ch);
        return 4;
    }

    const FilterParams fparams = filter_params_from_json(cfg);
    const std::uint64_t pixels =
        static_cast<std::uint64_t>(img.width) * static_cast<std::uint64_t>(img.height);

    std::vector<std::string> filters = opt.filters;
    if (filters.empty()) {
        filters.push_back("greyscale");
    }

    for (const std::string &fname : filters) {
        auto step_start = std::chrono::steady_clock::now();
        spdlog::info("Applying filter: {}", fname);
        if (!apply_filter_by_name(img, fname, fparams)) {
            spdlog::error("Unknown or failed filter: {}", fname);
            return 5;
        }
        auto step_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - step_start);
        if (opt.verbose) {
            spdlog::debug("Filter '{}' touched {} pixels in {} ms", fname,
                          pixels, step_ms.count());
        }
    }

    if (!write_image(opt.output_path, img, quality)) {
        spdlog::error("Failed to write output: {}", opt.output_path);
        return 6;
    }

    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    spdlog::info("Wrote {} ({}×{}, {} ms)", opt.output_path, img.width,
                 img.height, total_ms.count());
    return 0;
}

} // namespace pixelfrog
