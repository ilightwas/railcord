#ifndef LUCY_API_H
#define LUCY_API_H

#include <string>
#include <unordered_map>

namespace railcord {
    extern std::unordered_map<int, std::string> api_endpoints;

    enum api {
        sync_time_id = 0,
        personality_id = 1,
        worker_art_id = 2,
        license_id = 3
    };
}

#endif // !LUCY_API_H
