#include <cassert>
#include <sstream>

#include "gamedata.h"
#include "lucyapi.h"
#include "personality.h"
#include "util.h"

using namespace railcord::util;

namespace railcord {

const std::string personality::unknown_icon{"Unknown icon"};

const personality personality::unknown{
    personality::information{}, "Unkown", "Unkown personality", &personality::unknown_icon, &personality::unknown_icon};

std::string personality::information::str() const {
    std::stringstream ss;
    ss << "Id: " << id << ", effect: " << effect << ", type: " << ptype.t << ", art_id: " << art_id
       << ", goods_id: " << goods_id[0] << ' ' << goods_id[1];
    return ss.str();
}

std::string personality::str() const {
    std::stringstream ss;
    ss << "Name: " << name << ", desc: " << description << "\nInfo:[ " << info.str() << " ]";
    return ss.str();
}

personality::personality(personality::information i, std::string name, std::string description,
                         const std::string* icon1, const std::string* icon2)
    : info(i), name(std::move(name)), description(std::move(description)), main_icon(icon1), sec_icon(icon2) {}

std::string auction::str() const {
    std::stringstream ss{};
    ss << "Id: " << id << ", endTime: " << end_time.count() << ", personality_id: " << personality_id;
    return ss.str();
}

std::string personality::get_art_url() const {
    std::string tmp{api_endpoints.at(api::worker_art_id)};
    tmp.reserve(8);
    tmp.append(std::to_string(info.art_id)).append(".png");
    return tmp;
}

void from_json(const nlohmann::json& j, auction& au) {
    j.at("ID").get_to(au.id);
    au.end_time = std::chrono::seconds{as_int(j.at("endTime"))};
    au.personality_id = as_int(j.at("personality_id"));
}

void to_json(nlohmann::json&, const auction&) {}

void from_json(const nlohmann::json& j, personality::information& info) {
    typedef railcord::personality::type type;
    constexpr uint8_t max_know_types = type::unknown;

    info.id = as_int(j.at("ID"));
    int type_id = as_int(j.at("type"));
    info.ptype = type_id > max_know_types ? type::unknown : type(type_id);

    if (info.ptype == type::goods || info.ptype == type::hourly) {
        info.goods_id[0] = as_int(j.at("use1"));
        info.goods_id[1] = as_int(j.at("use2"));
    }

    info.effect = as_int(j.at("effect"));
    info.art_id = as_int(j.at("art"));
}

void to_json(nlohmann::json&, const personality::information&) {}

}   // namespace railcord
