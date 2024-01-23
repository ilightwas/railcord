#include <fmt/format.h>

#include "commands.h"
#include "logger.h"
#include "lucy.h"
#include "util.h"

namespace railcord::cmd {

using namespace std::chrono;

constexpr const char* prefix_license_bid = "license";
constexpr const char* cmd_option_name = "good_name";

License_Bid::License_Bid(Lucy* lucy)
    : Base_Cmd("license_bid", "Alert you when a license you want has an auction", seconds{3}, lucy) {

    license_manager_.update_state();
    last_update_ = system_clock::now();

    std::vector<std::string> choices;
    const auto& goods = lucy->gamedata()->goods();
    auto begin = goods.begin() + 1;
    auto end = goods.begin() + s_icon_effects_offset;
    while (begin != end) {
        choices.emplace_back(begin->name);
        ++begin;
    }

    // std::sort(choices_.begin(), choices_.end()); // for now index + 1 = good id

    std::vector<std::string> lowercase_choices;
    for (const auto& c : choices) {
        lowercase_choices.emplace_back(util::to_lowercase(c));
    }

    lucy->bot.on_autocomplete([bot = &lucy->bot, cmd_name = name_, choices = std::move(choices),
                               l_choices = std::move(lowercase_choices)](const dpp::autocomplete_t& event) {
        if (event.name != cmd_name) {
            return;
        }

        for (auto& opt : event.options) {
            /* The option which has focused set to true is the one the user is typing in */
            if (opt.name != cmd_option_name || !opt.focused) {
                continue;
            }

            std::string uservalue = util::to_lowercase(std::get<std::string>(opt.value));
            std::vector<int> autocomplete_choices;
            int idx = 0;
            int autocompleted = 0;
            for (const auto& c : l_choices) {
                if (util::starts_with(c, uservalue)) {
                    autocomplete_choices.push_back(idx);
                    ++autocompleted;
                }
                ++idx;
                if (autocompleted == 25) {
                    break;
                }
            }

            dpp::interaction_response res{dpp::ir_autocomplete_reply};
            for (int choice : autocomplete_choices) {
                res.add_autocomplete_choice(dpp::command_option_choice(
                    choices[static_cast<size_t>(choice)],
                    std::to_string(choice + 1)));   // good id is offset by one, first good ignored
            }

            bot->interaction_response_create(event.command.id, event.command.token, res);
            // logger->debug("Autocomplete {}  with value '{}' in field {} for user {}", opt.name, uservalue,
            // event.name,
            //               event.command.usr.global_name);
            break;
        }
    });
}

dpp::slashcommand License_Bid::build() {
    return dpp::slashcommand{name_, description_, lucy_->bot.me.id}.add_option(
        dpp::command_option(dpp::co_string, cmd_option_name, "Name of the good license", true).set_auto_complete(true));
}

void License_Bid::handle_slash_interaction(const dpp::slashcommand_t& event) {
    event.thinking();
    auto userchoice = std::get<std::string>(event.get_parameter(cmd_option_name));
    int good_type;

    try {
        good_type = util::as_int(userchoice);   // todo use a map later
    } catch (const std::exception&) {
        event.edit_original_response(
            dpp::message{fmt::format("\"{}\" is not a valid good", userchoice)}.set_flags(dpp::m_ephemeral));
        logger->warn("invalid good choice, User {} entered {}", event.command.usr.global_name, userchoice);
        return;
    }

    if (auto now = system_clock::now(); last_update_ + minutes{15} < now) {
        license_manager_.update_state();
        last_update_ = now;
    }

    const std::string& good_name = lucy_->gamedata()->goods()[static_cast<size_t>(good_type)].name;
    auto license = license_manager_.get_next_license(good_type);
    if (license) {

        if (license_manager_.is_currently_active(*license)) {
            event.edit_original_response(
                dpp::message{fmt::format("A license auction for {} is active right now!", good_name)});
            // .set_flags(dpp::m_ephemeral));
            return;
        }

        if (has_license_reminder(license->id, event.command.usr.id)) {
            event.edit_original_response(
                dpp::message{fmt::format("A reminder is already ongoing for the next license of {}", good_name)});
            // .set_flags(dpp::m_ephemeral));
            return;
        }

        add_reminder(*license, event.command.usr.id, event.command.channel_id);
        logger->debug("Added reminder for usr {}, {}, {}, {}", event.command.usr.global_name, good_name,
                      util::fmt_to_hr_min_sec(license_manager_.get_start_tp(*license) - system_clock::now()),
                      util::fmt_to_hr_min_sec(license_manager_.get_end_tp(*license) - system_clock::now()));

        event.edit_original_response(
            dpp::message{fmt::format("An auction for {} will start {}, I will remind you", good_name,
                                     util::timepoint_to_discord_timestamp(license_manager_.get_start_tp(*license)))});
        // .set_flags(dpp::m_ephemeral));
    } else {
        event.edit_original_response(dpp::message{fmt::format("No auction found for {}", good_name)});
        // .set_flags(dpp::m_ephemeral));
    }
}

std::optional<std::string> License_Bid::handler_prefix() { return {prefix_license_bid}; }

std::vector<dpp::snowflake> License_Bid::users_to_remind(const std::string& id) {
    std::lock_guard<std::mutex> lock{mtx_};
    auto found = license_reminders_.find(id);
    if (found != license_reminders_.end()) {
        return found->second;
    } else {
        return {};
    }
}

void License_Bid::add_reminder(const License& license, dpp::snowflake user, dpp::snowflake channel) {
    std::lock_guard<std::mutex> lock{mtx_};

    auto reminder = license_reminders_.find(license.id);
    if (reminder != license_reminders_.end()) {
        reminder->second.push_back(user);
    } else {
        auto inserted = license_reminders_.insert({license.id, {}});
        inserted.first->second.push_back(user);
    }

    if (has_active_reminder(license.id)) {
        return;
    }

    auto reminder_delay =
        duration_cast<seconds>(std::chrono::abs(license_manager_.get_end_tp(license) - system_clock::now()));

    if (reminder_delay > minutes{3}) {
        reminder_delay -= minutes{3};
    }

    logger->debug("Creating alert timer for id: {}, {}, by user {}", license.id, license.good_type,
                  static_cast<int64_t>(user));

    dpp::timer t = util::one_shot_timer(
        &lucy_->bot,
        [this, license, channel]() {
            License::Embed_Data eb{
                users_to_remind(license.id), &lucy_->gamedata()->goods()[static_cast<size_t>(license.good_type)],
                &license, license_manager_.get_end_tp(license)};

            dpp::message m = util::build_license_msg(&eb);
            m.set_channel_id(channel);
            lucy_->bot.message_create(m);
            remove_reminder(license.id);
            license_manager_.update_state();
        },
        static_cast<uint64_t>(reminder_delay.count()));
    active_timers_.insert({license.id, t});
}

void License_Bid::remove_reminder(const std::string& id) {
    std::lock_guard<std::mutex> lock{mtx_};
    license_reminders_.erase(id);
    active_timers_.erase(id);
}

bool License_Bid::has_license_reminder(const std::string& id, dpp::snowflake user) {
    std::lock_guard<std::mutex> lock{mtx_};
    auto license_alert = license_reminders_.find(id);
    if (license_alert != license_reminders_.end()) {
        for (const auto& u : license_alert->second) {
            if (user == u) {
                return true;
            }
        }
    }
    return false;
}

bool License_Bid::has_active_reminder(const std::string& id) { return active_timers_.find(id) != active_timers_.end(); }

}   // namespace railcord::cmd
