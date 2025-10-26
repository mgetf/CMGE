cmake_minimum_required(VERSION 3.15)
project(mge_tournament)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)

include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

set(SOURCES
    main.cpp
    tournament_manager.cpp
)

add_executable(mge_tournament ${SOURCES})

target_include_directories(mge_tournament PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

target_link_libraries(mge_tournament
    ${CURL_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    nlohmann_json::nlohmann_json
)

install(TARGETS mge_tournament DESTINATION bin)