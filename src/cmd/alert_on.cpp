#include <cassert>

#include <dpp/unicode_emoji.h>

#include "alert_manager.h"
#include "commands.h"
#include "logger.h"
#include "lucy.h"
#include "util.h"

namespace railcord::cmd {
using namespace std::chrono;

constexpr const char* prefix_alert_on = "alert";
constexpr const char* alert_on_enable = "enable";
constexpr const char* alert_on_back = "back";
constexpr const char* alert_on_setmsg = "setmsg";
constexpr const char* alert_on_preview = "preview";
constexpr const char* alert_on_timer = "timer";

static std::vector<dpp::component> build_personality_btns(Lucy* lucy, personality::type t) {
    Alert_Manager* alert_manager = lucy->alert_manager();
    const Gamedata* g = lucy->gamedata();

    bool alerts_enabled = alert_manager->is_alert_enabled(t);
    const dpp::emoji& emoji = g->get_emoji(t);
    std::vector<dpp::component> btns;

#pragma region back_btn
    {
        const dpp::emoji& back_emoji = lucy->custom_emojis().back();   // only one emoji now
        dpp::component back =
            dpp::component()
                .set_emoji(back_emoji.name, back_emoji.id)
                .set_style(dpp::cos_secondary)
                .set_type(dpp::cot_button)
                .set_id(build_id(prefix_alert_on, alert_on_back));
        btns.push_back(back);
    }

#pragma endregion back_btn

#pragma region enable_disable_btn
    {
        dpp::component enable_disable_alert;
        enable_disable_alert.set_style(alerts_enabled ? dpp::cos_danger : dpp::cos_success);
        enable_disable_alert.set_type(dpp::cot_button);
        enable_disable_alert.set_label(alerts_enabled ? "Disable Alerts" : "Enable Alerts");
        enable_disable_alert.set_emoji(emoji.name, emoji.id);
        enable_disable_alert.set_id(
            build_id(prefix_alert_on, alert_on_enable, std::to_string(t.t), std::to_string(alerts_enabled)));
        btns.push_back(enable_disable_alert);
    }

#pragma endregion enable_disable_btn

// set message
#pragma region set_alert_msg
    {
        dpp::component set_alert_msg;
        set_alert_msg.set_style(dpp::cos_success);
        set_alert_msg.set_type(dpp::cot_button);
        set_alert_msg.set_label("Message");
        set_alert_msg.set_emoji(dpp::unicode_emoji::page_facing_up);
        set_alert_msg.set_id(build_id(prefix_alert_on, alert_on_setmsg, std::to_string(t.t)));
        btns.push_back(set_alert_msg);
    }

#pragma endregion set_alert_msg

// preview alert
#pragma region preview_alert
    {
        dpp::component preview_alert;
        preview_alert.set_style(dpp::cos_primary);
        preview_alert.set_type(dpp::cot_button);
        preview_alert.set_label("Preview");
        preview_alert.set_emoji(dpp::unicode_emoji::eyes);
        preview_alert.set_disabled(alert_manager->get_alert_message(t).empty());
        preview_alert.set_id(build_id(prefix_alert_on, alert_on_preview, std::to_string(t.t)));

        btns.push_back(preview_alert);
    }
#pragma endregion preview_alert

// 5m, 10m, 20m, 30m, 1h
#pragma region timers
    {
        for (const int m : Alert_Info::s_intervals) {
            bool enabled = alert_manager->is_interval_enabled(t, m);
            dpp::component btn;
            btn.set_style(enabled ? dpp::cos_primary : dpp::cos_secondary);
            btn.set_type(dpp::cot_button);
            btn.set_label(std::to_string(m < 60 ? m : m / 60) + (m < 60 ? "m" : "h") + (enabled ? " (on)" : " (off)"));
            btn.set_emoji(dpp::unicode_emoji::alarm_clock);
            btn.set_id(build_id(prefix_alert_on, alert_on_timer, std::to_string(t.t), std::to_string(m),
                                std::to_string(enabled)));
            btns.push_back(btn);
        }
    }
#pragma endregion timers

    return btns;
}

static dpp::message build_select_menu_message(Lucy* lucy) {
    Gamedata* g = lucy->gamedata();
    Alert_Manager* alert_manager = lucy->alert_manager();
    const auto& goods = g->goods();
    assert(s_icon_effects_offset + s_personality_icons_sz == goods.size() - 1 &&
           "Goods size not consistent with static values");

    std::vector<dpp::select_option> options;
    auto iter = goods.begin() + s_icon_effects_offset + 1;

    uint8_t idx = 1;
    while (iter != goods.end()) {
        personality::type t{idx};
        bool enabled = alert_manager->is_alert_enabled(t);

        options.emplace_back(iter->name + (enabled ? " (on)" : " (off)"), std::to_string(idx), t.description())
            .set_emoji(iter->emoji, iter->emoji_id);
        ++idx;
        ++iter;
    }

    auto select_menu =
        dpp::component()
            .set_type(dpp::cot_selectmenu)
            .set_placeholder("Configure alerts")
            .set_id(std::string(prefix_alert_on) + "select");
    for (auto&& opt : options) {
        select_menu.add_select_option(opt);
    }

    dpp::message m{};
    m.add_component(dpp::component().add_component(select_menu));
    m.set_flags(dpp::m_ephemeral);

    return m;
}

Alert_on::Alert_on(Lucy* lu) : Base_Cmd("alert_on", "Alerts before worker auction ends", seconds{5}, lu) {}

dpp::slashcommand Alert_on::build() { return dpp::slashcommand(name_, description_, lucy_->bot.me.id); }

void Alert_on::handle_slash_interaction(const dpp::slashcommand_t& event) {
    event.reply(build_select_menu_message(lucy_));
}

void Alert_on::handle_button_click(const dpp::button_click_t& event) {

    const std::string& id = event.custom_id;
    Alert_Manager* alert_manager = lucy_->alert_manager();
    std::string param{id.substr(id.find(handler_prefix_sep) + 1)};
    logger->info("got param {}", param);
    auto args = extract_args(param);
    logger->info("got args {}", fmt::join(args, ","));

    if (util::starts_with(param, alert_on_back)) {
        event.reply(dpp::ir_update_message, build_select_menu_message(lucy_));
        return;
    } else if (util::starts_with(param, alert_on_setmsg)) {
        std::string modal_id = build_id(prefix_alert_on, alert_on_setmsg, args.front());
        personality::type t = personality::type{std::stoi(args.front())};
        dpp::interaction_modal_response modal(modal_id, t.description());

        modal.add_component(
            dpp::component()
                .set_label("Message")
                .set_id("msg_modal")
                .set_type(dpp::cot_text)
                .set_default_value(alert_manager->get_alert_message(t))
                .set_max_length(2000)
                .set_text_style(dpp::text_paragraph));

        event.dialog(modal);
        return;
    } else if (util::starts_with(param, alert_on_enable)) {
        personality::type t{std::stoi(args.front())};
        bool is_enabled = std::stoi(args.back());
        alert_manager->set_alert_enabled(t, !is_enabled);
        event.reply(dpp::ir_update_message, build_button_menu_msg(t));
        return;
    } else if (util::starts_with(param, alert_on_timer)) {
        personality::type t{std::stoi(args.front())};
        int interval = std::stoi(args[1]);
        bool enabled = std::stoi(args.back());
        alert_manager->set_alert_interval(t, interval, !enabled);
        event.reply(dpp::ir_update_message, build_button_menu_msg(t));
        return;
    } else if (util::starts_with(param, alert_on_preview)) {
        event.thinking();
        personality::type t{std::stoi(args.front())};
        event.edit_original_response(
            alert_manager->build_alert_message(
                lucy_->gamedata()->get_rnd_personality(t), system_clock::now() + minutes{5},
                alert_manager->get_alert_message(t), 5),
            [bot = &lucy_->bot](const dpp::confirmation_callback_t& cc) {
                if (!cc.is_error()) {
                    const dpp::message& m = cc.get<dpp::message>();
                    bot->start_timer(
                        [msg_id = m.id, ch_id = m.channel_id, bot](dpp::timer timer) {
                            logger->info("Deleting msg id = {} (preview)", static_cast<uint64_t>(msg_id));
                            bot->message_delete(msg_id, ch_id);
                            bot->stop_timer(timer);
                        },
                        util::s_delete_message_delay);
                }
            });
        return;
    }

    event.reply();
}

void Alert_on::handle_select_click(const dpp::select_click_t& event) {
    personality::type t{std::stoi(event.values[0])};
    event.reply(dpp::ir_update_message, build_button_menu_msg(t));
}

void Alert_on::handle_form_submit(const dpp::form_submit_t& event) {
    auto args = extract_args(event.custom_id);
    personality::type t{std::stoi(args.front())};
    std::string msg = std::get<std::string>(event.components[0].components[0].value);
    logger->info("sent by user: {}", event.command.usr.global_name);
    lucy_->alert_manager()->set_alert_message(t, msg);
    event.reply(dpp::ir_update_message, build_button_menu_msg(t));
}

std::optional<std::string> Alert_on::handler_prefix() { return {prefix_alert_on}; }

dpp::message Alert_on::build_button_menu_msg(personality::type t) {

    dpp::message m = lucy_->alert_manager()->build_alert_message(
        lucy_->gamedata()->get_rnd_personality(t), system_clock::now() + minutes{5},
        lucy_->alert_manager()->get_alert_message(t), 5);
    m.set_flags(dpp::m_ephemeral);

    auto btns = build_personality_btns(lucy_, t);
    dpp::component row;

    int btn_count = 0;
    for (const auto& btn : btns) {
        row.add_component(btn);
        if (++btn_count % 5 == 0) {
            m.add_component(row);
            row = dpp::component();
        }
    }

    m.embeds.front().set_color(lucy_->alert_manager()->is_alert_enabled(t) ? 0x00fd00 : 0xfd0000);
    return m;
}

}   // namespace railcord::cmd
