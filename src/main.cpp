#include "logger.h"
#include "processor.h"

#include <CLI/CLI.hpp>

#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace {

// When PIXELFROG_DEMO_XRAY_SECRET is defined: strings are intentionally fake (AWS-style ID, generic API key shape)
// and exist only so Xray / binary secret detectors have something to match. Never use real credentials here.
[[maybe_unused]] bool xray_demo_embedded_fake_credentials_touch() {
#ifdef PIXELFROG_DEMO_XRAY_SECRET
    static const char kFakeAwsAccessKeyId[] = "AKIAIOSFODNN7EXAMPLE";
    static const char kFakeApiToken[] =
        "xray-demo-jf_sk_live_0123456789abcdef0123456789abcdef0123456789";
    return std::strlen(kFakeAwsAccessKeyId) > 4 && std::strlen(kFakeApiToken) > 8;
#else
    return false;
#endif
}

} // namespace

int main(int argc, char **argv) {
    pixelfrog::ProcessOptions opt;
    std::vector<std::string> filter_names;
    std::optional<int> cli_quality;

    CLI::App app{"PixelFrog — C++ image processing demo"};
    app.require_subcommand(0, 0);
    app.add_option("-i,--input", opt.input_path, "Input image (JPEG or PNG)")
        ->required();
    app.add_option("-o,--output", opt.output_path, "Output path")
        ->default_val("output.jpg");
    app.add_option("-f,--filter", filter_names,
                   "Filter: greyscale|blur|edges|brighten|darken (repeatable)")
        ->expected(-1);
    app.add_option("-c,--config", opt.config_path, "JSON config override file");
    app.add_option("-q,--quality", cli_quality, "JPEG quality (1–100)")
        ->check(CLI::Range(1, 100));
    app.add_flag("-v,--verbose", opt.verbose, "Verbose logging");

    CLI11_PARSE(app, argc, argv);

    (void)xray_demo_embedded_fake_credentials_touch();

    if (!filter_names.empty()) {
        opt.filters = std::move(filter_names);
    }
    opt.jpeg_quality = cli_quality;

    pixelfrog::init_logger({.verbose = opt.verbose});
    const int rc = pixelfrog::run_processor(opt);
    pixelfrog::shutdown_logger();
    return rc;
}
