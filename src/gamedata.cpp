#include <cassert>
#include <deque>
#include <fstream>
#include <string>

#include <pugixml.hpp>

#include "gamedata.h"
#include "logger.h"

namespace railcord {

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
    logger->debug("Loading personality names and descriptions...");

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
        logger->error("Could not load personality names and descriptions: {}", res.description());
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

static std::deque<Good> load_goods(const std::string& goods_file) {
    logger->debug("Loading goods information...");

    std::ifstream stream{goods_file};

    if (!stream.is_open()) {
        throw std::runtime_error{"Could not open goods file"};
    }

    std::deque<Good> goods{};

    try {
        nlohmann::json j_goods = json::parse(stream);
        assert(j_goods.size() <= s_goods_arr_sz && "Goods json bigger than array");
        for (unsigned i = 0; i < s_goods_arr_sz; ++i) {
            const json& j = j_goods[i];
            goods.emplace_back(j["id"].get<int>(), j["name"].get<std::string>(), j["icon"].get<std::string>(),
                               j["emoji"].get<std::string>(), j["emoji-id"].get<std::uint64_t>());
        }

    } catch (const json::exception& e) {
        logger->error("Could not load goods information: {}", e.what());
        throw;
    }

    return goods;
}

void Gamedata::init(std::string p_stats_file, std::string gamedata_file, std::string goods_file) {
    pstats_map stats = load_personality_stats(p_stats_file);
    goods_ = load_goods(goods_file);
    const auto& [names, descriptions] = load_personality_names_desc(gamedata_file, stats);
    for (const auto& [id, stat] : stats) {
        const auto& icons = get_icons(stat);
        personalities_.insert({id, {stats[id], names.at(id), descriptions.at(id), icons.first, icons.second}});
    }
}

const personality& Gamedata::get_rnd_personality(personality::type t) const {

    for (const auto& [id, p] : personalities_) {
        if (p.info.ptype == t) {
            return p;
        }
    }
    // not found
    return personality::unknown;
}

dpp::emoji Gamedata::get_emoji(personality::type t) const {
    auto good = goods_[static_cast<size_t>(s_icon_effects_offset + t.t)];
    return dpp::emoji{good.emoji, good.emoji_id};
}

std::string Gamedata::get_effect_icon(personality::type t) const {
    auto good = goods_[static_cast<size_t>(s_icon_effects_offset + t.t)];
    return good.icon;
}

std::pair<const std::string*, const std::string*> Gamedata::get_icons(const personality::information& info) const {
    typedef personality::type t;
    switch (info.ptype) {
        case t::goods: {
            auto& g_ids = info.goods_id;
            size_t idx0{static_cast<size_t>(g_ids[0])};
            size_t idx1{static_cast<size_t>(g_ids[1])};
            assert(idx0 < goods_.size() && idx1 < goods_.size() && "Goods index out of bounds");

            return std::make_pair(&goods_[idx0].icon, &goods_[idx1 ? idx1 : idx0].icon);
        }
        case t::hourly: {
            auto&& icon = info.goods_id[0] ? &goods_[s_prestige_icon_idx].icon : &goods_[s_money_icon_idx].icon;
            return std::make_pair(icon, icon);
        }
        default: {
            size_t offset = s_icon_effects_offset + static_cast<size_t>(info.ptype);
            assert(offset < goods_.size() && "Goods index out of bounds");
            auto&& icon = &goods_[offset].icon;
            return std::make_pair(icon, icon);
        }
    }
}

}   // namespace railcord
