#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/md5.h>

#include <random>
#include <sstream>
#include <string>

#include "util.h"

namespace railcord::util {

std::string get_token(const std::string& token_file) {

    std::ifstream tk(token_file);
    if (!tk.is_open())
        throw std::runtime_error("Token file could not be loaded");

    std::string token;
    char a;
    while (tk.get(a)) {
        token.push_back(a);
    }

    return token;
}

std::string md5(const std::string& str) {
    unsigned char hash[MD5_DIGEST_LENGTH];

    MD5_CTX md5;
    MD5_Init(&md5);
    MD5_Update(&md5, str.c_str(), str.size());
    MD5_Final(hash, &md5);

    std::stringstream ss;

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

dpp::timer make_alert(dpp::cluster* bot, uint64_t seconds, int interval, const dpp::message& msg) {
    return bot->start_timer(
        [bot, msg, interval](dpp::timer t) {
            bot->message_create(msg, [bot, interval](const dpp::confirmation_callback_t& cc) {
                if (!cc.is_error()) {
                    const dpp::message& m = cc.get<dpp::message>();
                    auto delete_delay =
                        static_cast<uint64_t>((static_cast<unsigned>(interval) * 60u) + s_delete_message_delay -
                                              auction::s_discord_extra_delay.count());
                    bot->start_timer(
                        [msg_id = m.id, ch_id = m.channel_id, bot](dpp::timer timer) {
                            logger->info("Deleting msg id = {}", static_cast<uint64_t>(msg_id));
                            bot->message_delete(msg_id, ch_id);
                            bot->stop_timer(timer);
                        },
                        delete_delay);
                }
            });
            bot->stop_timer(t);
        },
        seconds);
}

dpp::embed build_embed(std::chrono::system_clock::time_point ends_at, const personality& p, bool with_timer) {
    using namespace std::chrono;

    dpp::embed e;
    const auto& art_url = p.get_art_url();
    if (with_timer) {
        std::string desc{};
        desc.reserve(64);
        desc.append("**# ").append(timepoint_to_discord_timestamp(ends_at)).append("**");
        e.set_description(desc);
    }

    e.add_field("", p.description);

    dpp::embed_author author;
    author.icon_url = art_url;
    author.name = p.name;
    e.set_author(author);

    dpp::embed_footer footer{};
    footer.set_text(s_inv_space);

    e.set_image(*p.main_icon);
    footer.set_icon(*p.sec_icon);

    e.set_footer(footer);
    e.set_timestamp(system_clock::to_time_t(ends_at));
    e.set_thumbnail(art_url);

    return e;
}

std::string fmt_http_request(const std::string& server, int port, const std::string& endpoint, bool https) {
    std::string url = https ? "https://" : "http://";
    const std::string str_port = std::to_string(port);
    url.reserve(server.length() + endpoint.length() + str_port.length() + 2);
    return url.append(server).append(":").append(str_port).append("/").append(endpoint);
}

uint32_t rnd_color() {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<uint32_t> dis;
    // Generate a random uint32_t value
    return dis(gen);
}

}   // namespace railcord::util
