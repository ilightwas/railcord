#include <memory>

#ifdef USE_SPDLOG

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "logger.h"

#else
#include "logger.h"
#endif

namespace railcord {

#ifdef USE_SPDLOG

std::shared_ptr<spdlog::logger> logger = []() -> auto {
    spdlog::init_thread_pool(8192, 2);

    std::vector<spdlog::sink_ptr> sinks;
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(stdout_sink);

    auto base_logger = std::make_shared<spdlog::async_logger>(
        "logs", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

    base_logger->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
    base_logger->set_level(spdlog::level::level_enum::trace);

    spdlog::register_logger(base_logger);
    return base_logger;
}();

void integrate_spdlog(dpp::cluster& bot, const std::string& log_file) {
    // not thread safe
    logger->sinks().emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file, 1024 * 1024 * 5, 10));

    /* Integrate spdlog logger to D++ log events */
    bot.on_log([](const dpp::log_t& event) {
        switch (event.severity) {
            case dpp::ll_trace:
                logger->trace("{}", event.message);
                break;
            case dpp::ll_debug:
                logger->debug("{}", event.message);
                break;
            case dpp::ll_info:
                logger->info("{}", event.message);
                break;
            case dpp::ll_warning:
                logger->warn("{}", event.message);
                break;
            case dpp::ll_error:
                logger->error("{}", event.message);
                break;
            case dpp::ll_critical:
                logger->critical("{}", event.message);
                break;
        }
    });
}

#else
std::shared_ptr<cout_logger> logger = std::make_shared<cout_logger>();
void init_logger() {}
#endif

}   // namespace railcord
