#pragma once

#include "filters.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace pixelfrog {

struct ProcessOptions {
    std::string input_path;
    std::string output_path = "output.jpg";
    std::vector<std::string> filters;
    std::string config_path;
    /// If set, overrides `output.jpeg_quality` from merged config.
    std::optional<int> jpeg_quality;
    bool verbose = false;
};

nlohmann::json default_config_json();

/// Deep-merge |overrides| on top of |base| (objects recurse; other types replace).
void json_merge_inplace(nlohmann::json &base, const nlohmann::json &overrides);

FilterParams filter_params_from_json(const nlohmann::json &cfg);

/// Runs full pipeline. Returns 0 on success, non-zero on error (message via spdlog).
int run_processor(const ProcessOptions &opt);

} // namespace pixelfrog
