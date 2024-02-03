#ifndef LOGGER_H
#define LOGGER_H

#include <string>

#include <dpp/dpp.h>

#ifdef USE_SPDLOG
#include <spdlog/spdlog.h>
#else
#include <fmt/core.h>
#include <iostream>
#endif

namespace railcord {

#ifdef USE_SPDLOG
extern std::shared_ptr<spdlog::logger> logger;
void integrate_spdlog(dpp::cluster& bot, const std::string& log_file);
#else
struct cout_logger {
    template <typename... Args>
    void trace(Args&&... args) {
        std::cout << fmt::format(std::forward<Args>(args)...) << std::endl;
    }

    template <typename... Args>
    void debug(Args&&... args) {

        std::cout << fmt::format(std::forward<Args>(arg)...) << std::endl;
    }

    template <typename... Args>
    void info(Args&&... args) {
        std::cout << fmt::format(std::forward<Args>(args)...) << std::endl;
    }

    template <typename... Args>
    void warn(Args&&... args) {
        std::cout << fmt::format(std::forward<Args>(args)...) << std::endl;
    }

    template <typename... Args>
    void error(Args&&... args) {
        std::cout << fmt::format(std::forward<Args>(args)...) << std::endl;
    }

    template <typename... Args>
    void critical(Args&&... args) {
        std::cout << fmt::format(std::forward<Args>(args)...) << std::endl;
    }
};

extern std::shared_ptr<cout_logger> logger;
#endif

}   // namespace railcord

#endif   // !LOGGER_H
