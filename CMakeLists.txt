cmake_minimum_required(VERSION 3.20.0)

project(lucy
  DESCRIPTION "A discord bot for Rail Nation"
  VERSION 1.0.0
  LANGUAGES CXX)

add_executable(lucy
  src/main.cpp
  src/logger.cpp
  src/personality.cpp
  src/gamedata.cpp
  src/lucy.cpp
  src/personality_watcher.cpp
  src/util.cpp
  src/alert_info.cpp
  src/alert_manager.cpp
  src/sent_messages.cpp
  src/license.cpp
  src/cmd/license_bid.cpp
  src/cmd/command_handler.cpp
  src/cmd/ping.cpp
  src/cmd/shutdown.cpp
  src/cmd/watch.cpp
  src/cmd/stop_watch.cpp
  src/cmd/set_channel.cpp
  src/cmd/alert_on.cpp
  src/cmd/save_settings.cpp
  src/cmd/remove_custom_message.cpp)

target_include_directories(lucy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(lucy PRIVATE cxx_std_17)
set_target_properties(lucy PROPERTIES CMAKE_CXX_EXTENSIONS OFF)

target_compile_options(lucy
  PRIVATE $<$<CXX_COMPILER_ID:MSVC>:
  /permissive- 
  /W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
  -Wall -Wextra -Wpedantic 
  -Wno-c++98-compat
  -Wno-switch-enum
  -Wno-exit-time-destructors 
  -Wno-global-constructors
  -Wno-shadow-field-in-constructor
  -Wno-c++98-compat-pedantic>)

target_compile_definitions(lucy
  PRIVATE USE_SPDLOG
  INTERFACE INI_STOP_ON_FIRST_ERROR=1)

find_package(dpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-inih CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cpr CONFIG REQUIRED)

target_link_libraries(lucy PRIVATE
  dpp::dpp
  fmt::fmt
  spdlog::spdlog
  unofficial::inih::inireader
  pugixml::pugixml
  OpenSSL::Crypto
  cpr::cpr)
