cmake_minimum_required(VERSION 3.16)
project(live_share_safety_test LANGUAGES CXX)

enable_testing()

add_executable(live_share_safety_test
    live_share_safety_test.cpp
    ../../app/libs/live_share_safety/live_share_safety.cpp
)

target_include_directories(live_share_safety_test PRIVATE
    ../../app/libs/live_share_safety
)
target_compile_features(live_share_safety_test PRIVATE cxx_std_11)
target_compile_options(live_share_safety_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME live_share_safety_test COMMAND live_share_safety_test)
