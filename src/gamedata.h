#ifndef GAMEDATA_H
#define GAMEDATA_H

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <dpp/dpp.h>

#include "personality.h"

namespace railcord {

using PIcons = std::pair<const std::string*, const std::string*>;
using GameIconUrl = std::string;

struct GameIconEmoji {
    std::uint64_t id;
    std::string name;
};

struct GameResource {
    int id;
    std::string name;
    GameIconUrl icon_url;
    std::optional<GameIconEmoji> emoji;
};

using Good = GameResource;
using PersonalityEffect = GameResource;

void from_json(const nlohmann::json& j, GameResource& gr);
void to_json(nlohmann::json& j, const GameResource& gr);

class GameData {
  public:
    void init(const std::string& game_mode = "classic");

    const personality& get_personality(int id) const;
    const personality& get_rnd_personality(personality::type t) const;

    const std::deque<Good>& goods() const;
    const std::deque<Good>& personality_effects() const;
    const Good* get_license_good(int good_type) const;

    dpp::emoji get_emoji(personality::type t) const;

  private:
    PIcons get_icons(const personality::information& info) const;
    personality unknow_;
    GameIconUrl err_icon_;
    std::unordered_map<int, personality> personalities_;
    std::deque<Good> goods_;
    std::deque<PersonalityEffect> pEffects_;
};

}   // namespace railcord

#endif   // !GAMEDATA_H
