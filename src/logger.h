#pragma once

#include <memory>

namespace pixelfrog {

struct LoggerConfig {
    bool verbose = false;
};

void init_logger(const LoggerConfig &cfg);
void shutdown_logger();

} // namespace pixelfrog
