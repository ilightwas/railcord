#include <cassert>
#include <fstream>

#include <pugixml.hpp>

#include "gamedata.h"
#include "logger.h"

namespace railcord {

#define DESCRIPTION_PLACEHOLDER "%0"
#define RESOURCE_PATH(g, r) (std::string{}.append("resources/").append(g).append(r))

inline constexpr char s_name_fmt[]{"IDS_PERSONALITY_NAME_"};
inline constexpr char s_effect_fmt[]{"IDS_PERSONALITY_EFFECT_"};

typedef std::unordered_map<int, std::string> str_map;
typedef std::unordered_map<int, personality::information> pstats_map;
typedef std::pair<str_map, str_map> names_desc;
using json = nlohmann::json;

static pstats_map load_personality_stats(const std::string& stats_file) {
    logger->debug("Loading personality stats...");
    std::ifstream stat_file{stats_file};

    if (!stat_file.is_open()) {
        throw std::runtime_error{"Could not open personalities stats file"};
    }

    nlohmann::json json_stats = nlohmann::json::parse(stat_file);
    pstats_map stats;

    for (auto&& json_info : json_stats) {
        personality::information&& info = json_info.get<personality::information>();
        stats.emplace(info.id, info);
    }

    return stats;
}

static names_desc load_personality_names_desc(const std::string& gamedata_file, const pstats_map& p_stats) {
    typedef railcord::personality::type type;
    logger->debug("Parsing personality names and descriptions...");

    // skip extra personalities not used in the current stats data file
    auto not_found = [](int id, auto& map) -> bool {
        auto iter = map.find(id);
        return iter == map.end();
    };

    auto starts_with = [](const auto& needle, const auto& value) -> bool {
        return ::strncmp(needle, value, ::strlen(needle)) == 0;
    };

    // pray for no unicod/wide strs o_o
    auto get_id = [](auto str, auto sz) -> int { return std::atoi(static_cast<const char*>(&(str)[sz])); };

    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(gamedata_file.c_str());

    if (!res) {
        logger->error("Could not parse personality names and descriptions: {}", res.description());
        throw std::runtime_error{res.description()};
    }

    pugi::xml_node items = doc.child("tmx").child("body");
    std::unordered_map<int, std::string> all_names;
    std::unordered_map<int, std::string> all_descriptions;

    for (auto item : items) {
        for (auto attr : item.attributes()) {
            if (starts_with(s_name_fmt, attr.as_string())) {

                int id = get_id(attr.as_string(), sizeof(s_name_fmt) - 1);

                if (not_found(id, p_stats)) {
                    continue;
                }

                all_names.insert(std::make_pair(id, item.child("tuv").child("seg").text().get()));

            } else if (starts_with(s_effect_fmt, attr.as_string())) {
                int id = get_id(attr.as_string(), sizeof(s_effect_fmt) - 1);

                if (not_found(id, p_stats)) {
                    continue;
                }

                const auto& info = p_stats.at(id);
                std::string description = item.child("tuv").child("seg").text().get();

                unsigned delete_percent{};

                switch (info.ptype.t) {
                    case type::hourly:
                        [[fallthrough]];
                    case type::speed:
                        [[fallthrough]];
                    case type::research:
                        ++delete_percent;
                        break;
                    default:
                        break;
                }

                auto placeholder_idx = description.find(DESCRIPTION_PLACEHOLDER);
                if (placeholder_idx != std::string::npos) {
                    description.erase(placeholder_idx + 1 - delete_percent, 1 + delete_percent);
                    description.insert(placeholder_idx, std::to_string(info.effect));
                }
                all_descriptions.insert(std::make_pair(id, std::move(description)));
            }
        }
    }

    return {all_names, all_descriptions};
}

static std::deque<GameResource> load_gameresource(const std::string& file_name) {
    logger->debug("Loading file {}", file_name);

    std::ifstream stream{file_name};
    if (!stream.is_open()) {
        throw std::runtime_error{fmt::format("Could not open {} file", file_name)};
    }

    try {
        return json::parse(stream).get<std::deque<GameResource>>();
    } catch (const json::exception& e) {
        logger->error("{}", e.what());
        throw;
    }
}

void GameData::init(const std::string& game_mode) {
    pstats_map stats = load_personality_stats(RESOURCE_PATH(game_mode, "_personalities.json"));
    goods_ = load_gameresource(RESOURCE_PATH(game_mode, "_goods.json"));
    pEffects_ = load_gameresource(RESOURCE_PATH(game_mode, "_personality_effects.json"));
    const auto& [names, descriptions] = load_personality_names_desc(RESOURCE_PATH(game_mode, "_gamedata.xml"), stats);
    for (const auto& [id, stat] : stats) {
        const auto& [main, sec] = get_icons(stat);
        personalities_.try_emplace(id, stats.at(id), names.at(id), descriptions.at(id), main, sec);
    }

    assert(goods_.size() > 0);
    unknow_ = personality{personality::information{}, "Unkown", "Unkown personality", &goods_.front().icon_url,
                          &goods_.front().icon_url};
}

const personality& GameData::get_personality(int id) const { return personalities_.at(id); }

const personality& GameData::get_rnd_personality(personality::type t) const {

    for (const auto& [id, p] : personalities_) {
        if (p.info.ptype == t) {
            return p;
        }
    }
    // not found
    return unknow_;
}

const std::deque<Good>& GameData::goods() const { return goods_; }

const std::deque<Good>& GameData::personality_effects() const { return pEffects_; }

dpp::emoji GameData::get_emoji(personality::type t) const {
    assert(t.t < pEffects_.size());
    const auto& effects = pEffects_[static_cast<size_t>(t.t)];
    if (effects.emoji) {
        return {effects.emoji->name, effects.emoji->id};
    }
    return {};
}

const Good* GameData::get_license_good(int good_type) const {
    size_t idx{static_cast<size_t>(good_type)};

    if (goods_.size() > idx) {
        return &goods_[idx];
    } else {
        return &goods_.front();
    }
}

PIcons GameData::get_icons(const personality::information& info) const {
    typedef personality::type t;
    switch (info.ptype) {
        case t::goods: {
            auto& g_ids = info.goods_id;
            size_t idx0{static_cast<size_t>(g_ids[0])};
            size_t idx1{static_cast<size_t>(g_ids[1])};
            assert(idx0 < goods_.size() && idx1 < goods_.size() && "Goods index out of bounds");

            return std::make_pair(&goods_[idx0].icon_url, &goods_[idx1 ? idx1 : idx0].icon_url);
        }
        case t::hourly: {
            const GameResource* res = nullptr;
            const auto find_money = [](const GameResource& g) { return g.name == "Money"; };
            const auto find_prestige = [](const GameResource& g) { return g.name == "Hourly prestige"; };

            switch (info.goods_id[0]) {
                case 0: {
                    auto money = std::find_if(goods_.begin(), goods_.end(), find_money);
                    if (money != goods_.end()) {
                        res = &(*money);
                    }
                    break;
                }
                case 2: {
                    auto prestige = std::find_if(pEffects_.begin(), pEffects_.end(), find_prestige);
                    if (prestige != pEffects_.end()) {
                        res = &(*prestige);
                    }
                    break;
                }
                default: {
                    logger->warn("No hourly personality found with value {}", info.goods_id[0]);
                }
            }
            if (!res) {
                logger->warn("No resource of hourly personality with id {} found", info.id);
                return std::make_pair(&err_icon_, &err_icon_);
            }

            return std::make_pair(&res->icon_url, &res->icon_url);
        }
        default: {
            assert(info.ptype.t < pEffects_.size() && "pEffects index out of bounds");
            auto&& icon = &pEffects_[info.ptype.t].icon_url;
            return std::make_pair(icon, icon);
        }
    }
}

void from_json(const nlohmann::json& j, GameResource& gr) {
    const auto emoji_id = "emoji-id";
    const auto emoji_name = "emoji";

    gr.id = j.at("id");
    gr.name = j.at("name");
    gr.icon_url = j.at("icon");

    if (j.contains(emoji_id) && j.contains(emoji_name)) {
        gr.emoji = {j.at(emoji_id), j.at(emoji_name)};
    }
}

void to_json(nlohmann::json&, const GameResource&) {}

}   // namespace railcord
