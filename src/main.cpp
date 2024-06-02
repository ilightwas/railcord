#include "logger.h"
#include "lucy.h"

int main(int argc, const char* argv[]) {

    railcord::Lucy lucy{};
    try {
        lucy.init(argc, argv);
    } catch (const std::exception& e) {
        railcord::logger->error("Failed to init Lucy: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
