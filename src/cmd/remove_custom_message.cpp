#include <dpp/unicode_emoji.h>

#include "commands.h"
#include "logger.h"
#include "lucy.h"
#include "util.h"

namespace railcord::cmd {
using namespace std::chrono;

constexpr const char* prefix_remove_custom_message = "removecmsg";
constexpr const char* remove_custom_message_select_click = "rmcmselect";

Remove_Custom_Message::Remove_Custom_Message(Lucy* lucy)
    : Base_Cmd("remove_custom_message", "Remove a custom message for the alerts", seconds{3}, lucy) {}

dpp::slashcommand Remove_Custom_Message::build() { return dpp::slashcommand(name_, description_, lucy_->bot.me.id); }

static dpp::message build_custom_message_menu(Lucy* lucy) {
    Alert_Manager* alert_manager = lucy->alert_manager();
    std::vector<dpp::select_option> options;
    auto custom_msgs = alert_manager->get_custom_msgs();

    if (custom_msgs.empty()) {
        return dpp::message{"No custom messages set"}.set_flags(dpp::m_ephemeral);
    }

    for (const auto& msg : custom_msgs) {
        options.emplace_back(msg.title, std::to_string(msg.id), msg.body.substr(0, 64).append("..."))
            .set_emoji(dpp::unicode_emoji::x);
    }

    auto select_menu =
        dpp::component()
            .set_type(dpp::cot_selectmenu)
            .set_placeholder("Remove message")
            .set_id(build_id(prefix_remove_custom_message, remove_custom_message_select_click));

    for (auto&& opt : options) {
        select_menu.add_select_option(opt);
    }

    dpp::message m{};
    m.add_component(dpp::component().add_component(select_menu));
    m.set_flags(dpp::m_ephemeral);

    return m;
}

void Remove_Custom_Message::handle_slash_interaction(const dpp::slashcommand_t& event) {
    event.reply(build_custom_message_menu(lucy_));
}

void Remove_Custom_Message::handle_select_click(const dpp::select_click_t& event) {
    int id = std::stoi(event.values[0]);
    lucy_->alert_manager()->remove_custom_message(id);
    logger->info("Removing custom msg id={} by user {}", id, event.command.usr.global_name);
    event.reply(dpp::ir_update_message, build_custom_message_menu(lucy_));
}

std::optional<std::string> Remove_Custom_Message::handler_prefix() { return {prefix_remove_custom_message}; }

}   // namespace railcord::cmd
