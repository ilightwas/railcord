#define OPENSSL_SUPPRESS_DEPRECATED

#ifdef WIN32
// disable msvc complaining about 'localtime' deprecated
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <sstream>

#include <openssl/md5.h>

#include "gamedata.h"
#include "sent_messages.h"
#include "util.h"

namespace railcord::util {

using json = nlohmann::json;
using namespace std::chrono;

static std::mutex localtime_mtx;

std::tm get_localtime(std::time_t* tt) {
    std::lock_guard<std::mutex> lock{localtime_mtx};
    return *std::localtime(tt);
}

system_clock::duration left_to_next_hour(std::chrono::system_clock::time_point tp) {
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm{get_localtime(&tt)};
    return minutes{(60 - tm.tm_min)} - seconds{tm.tm_sec};
}

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

dpp::timer make_alert(dpp::cluster* bot, const Alert_Data& data, Sent_Messages* sent_msgs) {

    auto delete_delay = static_cast<uint64_t>(
        (static_cast<unsigned>(data.interval) * 60u) + s_delete_message_delay - auction::s_discord_extra_delay.count());

    auto delete_msg = [bot, sent_msgs, delete_delay](const dpp::confirmation_callback_t& cc) {
        if (!cc.is_error()) {
            const dpp::message& m = cc.get<dpp::message>();
            sent_msgs->add_message(m.id, m.channel_id);

            util::one_shot_timer(
                bot, [msg_id = m.id, sent_msgs]() { sent_msgs->delete_message(msg_id, "(alert)"); }, delete_delay);
        }
    };

    auto send_msg = std::make_shared<std::function<void()>>([bot, delete_msg, msg = data.msg]() {
        bot->message_create(msg, delete_msg);
    });

    return util::one_shot_timer(
        bot, [send_msg]() { (*send_msg)(); }, data.seconds);
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

dpp::message build_license_msg(License::Embed_Data* eb) {
    dpp::embed e;
    // e.set_image(good->icon);
    e.set_thumbnail(eb->good->icon);
    e.add_field("License", fmt::format("The auction for {} will end soon {}", eb->good->name,
                                       timepoint_to_discord_timestamp(eb->end_tp)));
    e.add_field("Minimum price", std::to_string(eb->license->min_price));
    e.add_field("Amount", std::to_string(eb->license->count).append("x"));

    dpp::embed_author author;
    author.icon_url = eb->good->icon;
    author.name = eb->good->name;
    e.set_author(author);

    dpp::message m;
    m.allowed_mentions.parse_users = true;
    std::string mentions;
    mentions.reserve(eb->users.size() * 10);
    for (const auto& u : eb->users) {
        mentions.append(user_mention(u));
    }

    m.set_content(mentions);
    m.add_embed(e);

    return m;
}

std::string fmt_http_request(const std::string& server, int port, const std::string& endpoint, bool https) {
    std::string url = https ? "https://" : "http://";
    const std::string str_port = std::to_string(port);
    url.reserve(server.length() + endpoint.length() + str_port.length() + 2);
    return url.append(server).append(":").append(str_port).append("/").append(endpoint);
}

uint32_t rnd_color() {
    // Generate a random uint32_t value
    return rnd_gen([&](auto& gen) {
        std::uniform_int_distribution<uint32_t> dis;
        return dis(gen);
    });
}

std::string rnd_emoji(uint32_t idx) {
    static const nlohmann::json emojis = []() {
        std::ifstream f{"resources/emojis.json"};
        try {
            return nlohmann::json::parse(f);
        } catch (const json::exception& e) {
            logger->warn("Parsing emoji failed with: {}", e.what());
            return nlohmann::json{};
        }
    }();

    static const std::vector<std::string> keys = [&]() {
        std::vector<std::string> k;
        for (auto it = emojis.begin(); it != emojis.end(); ++it) {
            k.push_back(it.key());
        }
        return k;
    }();

    if (idx) {
        return emojis[keys[idx % (keys.size() - 1ull)]];
    }
    // Generate a random index within the vector's bounds
    std::uniform_int_distribution<uint32_t> distribution(
        0, std::max<uint32_t>(0, static_cast<uint32_t>(keys.size() - 1ull)));
    uint32_t rnd_idx = rnd_gen([&](auto& gen) { return distribution(gen); });

    return emojis[keys[rnd_idx]];
}

uint32_t rnd_gen(std::function<uint32_t(std::mt19937& gen)> f) {
    static std::mutex mtx;
    static std::mt19937 gen(std::random_device{}());

    std::lock_guard<std::mutex> lock{mtx};
    return f(gen);
}

}   // namespace railcord::util
