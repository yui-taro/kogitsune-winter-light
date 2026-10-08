cmake_minimum_required(VERSION 3.22)
project(KogitsuneWinterLight LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib 6.0 QUIET)
if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(raylib)
endif()

find_package(CURL REQUIRED)

add_executable(foxgame src/main.cpp src/World.cpp src/JevBridge.cpp src/Audio.cpp)
target_include_directories(foxgame PRIVATE include vendor)
target_link_libraries(foxgame PRIVATE raylib CURL::libcurl)
target_compile_definitions(foxgame PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
add_custom_command(TARGET foxgame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:foxgame>/assets)

enable_testing()
add_executable(test_world ../tests/test_world.cpp src/World.cpp)
target_include_directories(test_world PRIVATE include)
add_test(NAME world_invariants COMMAND test_world)
add_executable(test_contract ../tests/test_contract.cpp src/World.cpp)
target_include_directories(test_contract PRIVATE include)
add_test(NAME design_contract COMMAND test_contract)
