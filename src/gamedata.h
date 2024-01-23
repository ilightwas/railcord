#ifndef GAMEDATA_H
#define GAMEDATA_H

#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "personality.h"

namespace railcord {

#define DESCRIPTION_PLACEHOLDER "%0"

inline constexpr const char* s_personalities_file{"resources/gamedata.xml"};
inline constexpr const char* s_personalities_stats_file{"resources/personalities.json"};
inline constexpr const char* s_goods_file{"resources/goods.json"};
inline constexpr const char* s_auction_ids_file = "auctions.txt";
inline constexpr char s_name_fmt[]{"IDS_PERSONALITY_NAME_"};
inline constexpr char s_effect_fmt[]{"IDS_PERSONALITY_EFFECT_"};

inline constexpr int s_personality_icons_sz = 18;
inline constexpr int s_goods_arr_sz = 51 + s_personality_icons_sz;   // 51 goods, 18 personality icons
inline constexpr int s_icon_effects_offset = 50;
inline constexpr int s_money_icon_idx = 50;
inline constexpr int s_prestige_icon_idx = 56;

struct Good {
    Good(int id, std::string n, std::string ic, std::string em, std::uint64_t em_id)
        : id(id), name(std::move(n)), icon(std::move(ic)), emoji(std::move(em)), emoji_id(em_id) {}
    int id;
    std::string name;
    std::string icon;
    std::string emoji;
    std::uint64_t emoji_id;
};

class Gamedata {
  public:
    void init(std::string p_stats_file = s_personalities_stats_file, std::string gamedata_file = s_personalities_file,
              std::string goods_file = s_goods_file);

    const personality& get_personality(int id) const { return personalities_.at(id); }
    const personality& get_rnd_personality(personality::type t) const;

    const std::deque<Good>& goods() const { return goods_; }

    dpp::emoji get_emoji(personality::type t) const;
    std::string get_effect_icon(personality::type t) const;

  private:
    std::pair<const std::string*, const std::string*> get_icons(const personality::information& info) const;
    std::unordered_map<int, personality> personalities_;
    std::deque<Good> goods_;
};

std::unordered_set<std::string> load_auction_ids();
void save_auction_ids(const std::unordered_set<std::string>& auction_ids);

}   // namespace railcord

#endif   // !GAMEDATA_H
