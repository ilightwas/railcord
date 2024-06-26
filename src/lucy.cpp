#include <mutex>

#include <ini.h>
#include <INIReader.h>

#include "cmd/commands.h"
#include "cmdline_helper.h"
#include "gamedata.h"
#include "logger.h"
#include "lucy.h"
#include "lucyapi.h"
#include "personality_watcher.h"
#include "util.h"

namespace railcord {

dpp::snowflake test_server{};
dpp::snowflake Lucy::s_bot_owner;
std::unordered_map<int, std::string> api_endpoints;

Lucy::Lucy() : Lucy(railcord::util::get_token(token_file)) {}

Lucy::Lucy(const std::string& token)
    : bot(token), alert_manager_(&bot), watcher_(&bot, &gamedata_, &alert_manager_), cmd_handler_(this) {}

void Lucy::init(int argc, const char* argv[]) {
#ifdef USE_SPDLOG
    integrate_spdlog(bot, "lucy.log");
#else
    bot.on_log(dpp::utility::cout_logger());
#endif
    load_settings();

    const auto action = cmd::parse_cmdline(argc, argv);
    if (action != cmd::BotAction::INIT) {
        cmd::do_cmdline_action(action, this);
    } else {
        gamedata_.init();

        cmd_handler_.load_all_commands();
        cmd_handler_.on_slash_cmd();
        cmd_handler_.on_form_submit();
        cmd_handler_.on_button_click();
        cmd_handler_.on_select_click();
    }

    running_.store(true);
    bot.start();

    {
        std::mutex thread_mutex;
        auto block_calling_thread = [this, &thread_mutex]() -> void {
            std::unique_lock<std::mutex> thread_lock(thread_mutex);
            bot.terminating.wait(thread_lock);
        };

        block_calling_thread();
    }
}

void Lucy::load_settings() {
    logger->debug("Loading lucy settings...");
    INIReader* settings = new INIReader("lucy.ini");

    if (int err = settings->ParseError()) {
        if (err == -1) {
            throw std::runtime_error("Could not load settings file");
        } else {
            throw std::runtime_error(fmt::format("Parse error at line {} of settings file", err));
        }
    }

    s_bot_owner = settings->GetUnsigned64("Lucy", "owner", 0);

    std::string server = settings->Get("Lucy", "api_server", "127.0.0.1");
    int port = settings->GetInteger("Lucy", "api_port", 6969);
    bool https = settings->GetInteger64("Lucy", "https", 0);

    std::string url = util::fmt_http_request(server, port, settings->Get("Lucy", "sync_time_endpoint", ""), https);
    api_endpoints.insert(std::make_pair(api::sync_time_id, url));

    url = util::fmt_http_request(server, port, settings->Get("Lucy", "personality_endpoint", ""), https);
    api_endpoints.insert(std::make_pair(api::personality_id, url));

    url = util::fmt_http_request(server, port, settings->Get("Lucy", "license_endpoint", ""), https);
    api_endpoints.insert(std::make_pair(api::license_id, url));

    api_endpoints.insert(std::make_pair(api::worker_art_id, settings->Get("Lucy", "worker_art_endpoint", "")));

    alert_manager_.set_alert_channel(settings->GetUnsigned64("Lucy", "channel", 0));
    alert_manager_.set_alert_role(settings->GetUnsigned64("Lucy", "alert_role", 0));

    bot_admin_role_ = settings->GetUnsigned64("Lucy", "bot_admin_role", 0);

    if (bot_admin_role_.empty()) {
        logger->warn("Bot admin role default initialized to 0");
    }

    watcher_.set_using_local_time(settings->GetInteger("Lucy", "use_local_time", 1));

    test_server = settings->GetUnsigned64("Lucy", "test_server", 0);

    whitelist_.push_back(settings->GetUnsigned64("Lucy", "user1", 0));
    whitelist_.push_back(settings->GetUnsigned64("Lucy", "user2", 0));
    whitelist_.push_back(s_bot_owner);
    custom_emojis_.emplace_back("rnback", settings->GetUnsigned64("Lucy", "rnback", 0));

    delete settings;

    if (alert_manager_.load_state()) {
        logger->info("Alert manager state loaded");
    } else {
        throw std::runtime_error{"Failed to load the alert manager state"};
    }
}

void Lucy::shutdown() {
    static std::once_flag s_flag;
    std::call_once(s_flag, [this]() {
        alert_manager_.save_state();
        (void) std::async(std::launch::async, [this]() {
            running_.store(false);
            std::this_thread::sleep_for(std::chrono::seconds{5});
            bot.terminating.notify_one();
        });
    });
}

}   // namespace railcord
