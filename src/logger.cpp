#include "logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace pixelfrog {

namespace {

std::shared_ptr<spdlog::logger> g_logger;

} // namespace

void init_logger(const LoggerConfig &cfg) {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    g_logger = std::make_shared<spdlog::logger>("pixelfrog", sink);
    spdlog::set_default_logger(g_logger);
    g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    if (cfg.verbose) {
        g_logger->set_level(spdlog::level::debug);
    } else {
        g_logger->set_level(spdlog::level::info);
    }
}

void shutdown_logger() { spdlog::shutdown(); }

} // namespace pixelfrog
