#ifndef COMMANDS_H
#define COMMANDS_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <dpp/dpp.h>

#include "personality.h"

namespace railcord {
class Lucy;
}

namespace railcord::cmd {

inline constexpr const char* handler_prefix_sep = ":";
inline constexpr const char* handler_arg_sep = "-";

class Base_Cmd {
  public:
    Base_Cmd() = default;
    Base_Cmd(const Base_Cmd&) = default;
    Base_Cmd(Base_Cmd&&) = default;
    Base_Cmd& operator=(const Base_Cmd&) = default;
    Base_Cmd& operator=(Base_Cmd&&) = default;
    virtual ~Base_Cmd() = default;

    virtual dpp::slashcommand build() = 0;
    virtual void handle_slash_interaction(const dpp::slashcommand_t&) {}
    virtual void handle_button_click(const dpp::button_click_t&) {}
    virtual void handle_select_click(const dpp::select_click_t&) {}
    virtual void handle_form_submit(const dpp::form_submit_t&) {}

    virtual std::optional<std::string> handler_prefix() { return {}; }

    const std::string& name() { return name_; }
    const std::string& description() { return description_; }
    const std::chrono::seconds cooldown() { return cooldown_; }

    bool is_on_cooldown() { return std::chrono::system_clock::now() < last_run_ + cooldown_; }
    void update_last_run_time() { last_run_ = std::chrono::system_clock::now(); }

  protected:
    Base_Cmd(std::string n, std::string d, std::chrono::duration<long> c, Lucy* lucy)
        : name_(std::move(n)), description_(std::move(d)), cooldown_(c), lucy_(lucy) {}
    std::string name_;
    std::string description_;
    std::chrono::seconds cooldown_;
    std::chrono::system_clock::time_point last_run_;
    Lucy* lucy_;
};

class Ping : public Base_Cmd {
  public:
    Ping(Lucy* lucy);

    dpp::slashcommand build() override;
    void handle_slash_interaction(const dpp::slashcommand_t& event) override;
};

class Shutdown : public Base_Cmd {
  public:
    Shutdown(Lucy* lucy);

    dpp::slashcommand build() override;
    void handle_slash_interaction(const dpp::slashcommand_t& event) override;
};

class Watch : public Base_Cmd {
  public:
    Watch(Lucy* lucy);

    dpp::slashcommand build() override;
    void handle_slash_interaction(const dpp::slashcommand_t& event) override;
};

class Stop_watch : public Base_Cmd {
  public:
    Stop_watch(Lucy* lucy);

    dpp::slashcommand build() override;
    void handle_slash_interaction(const dpp::slashcommand_t& event) override;
};

class Set_channel : public Base_Cmd {
  public:
    Set_channel(Lucy* lucy);

    dpp::slashcommand build() override;
    void handle_slash_interaction(const dpp::slashcommand_t& event) override;
};

class Alert_on : public Base_Cmd {
  public:
    Alert_on(Lucy* lucy);

    dpp::slashcommand build() override;
    void handle_slash_interaction(const dpp::slashcommand_t& event) override;
    void handle_button_click(const dpp::button_click_t&) override;
    void handle_select_click(const dpp::select_click_t&) override;
    void handle_form_submit(const dpp::form_submit_t&) override;
    std::optional<std::string> handler_prefix() override;

  private:
    dpp::message build_button_menu_msg(personality::type t);
};

class Save_Settings : public Base_Cmd {
  public:
    Save_Settings(Lucy* lucy);

    dpp::slashcommand build() override;
    void handle_slash_interaction(const dpp::slashcommand_t& event) override;
};

class Remove_Custom_Message : public Base_Cmd {
  public:
    Remove_Custom_Message(Lucy* lucy);

    dpp::slashcommand build() override;
    void handle_slash_interaction(const dpp::slashcommand_t& event) override;
    void handle_select_click(const dpp::select_click_t&) override;
    std::optional<std::string> handler_prefix() override;
};

template <typename Arg1, typename Arg2, typename... Params>
std::string build_id(Arg1 prefix, Arg2 handler, Params&&... params) {
    std::string id{prefix};
    id.append(handler_prefix_sep);
    id.append(handler);
    if constexpr (sizeof...(Params) != 0) {
        (id.append(handler_arg_sep).append(std::forward<Params>(params)), ...);
    }
    return id;
}

inline std::vector<std::string> extract_args(const std::string& id) {
    std::vector<std::string> args;
    auto arg = id.find(handler_arg_sep);
    while (arg != std::string::npos && arg + 1 < id.size()) {
        auto end = id.find(handler_arg_sep, arg + 1);
        std::string&& str = end != std::string::npos ? id.substr(arg + 1, end - (arg + 1)) : id.substr(arg + 1);
        if (!str.empty()) {
            args.push_back(std::move(str));
        }
        arg = end;
    }

    return args;
}

}   // namespace railcord::cmd

#endif   // !COMMANDS_H
