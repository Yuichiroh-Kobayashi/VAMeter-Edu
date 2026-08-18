cmake_minimum_required(VERSION 3.16)
project(live_share_controller_test LANGUAGES CXX)

enable_testing()

add_executable(live_share_controller_test
    live_share_controller_test.cpp
    ../../app/libs/live_share_controller/live_share_controller.cpp
    ../../app/libs/live_share_session/live_share_session.cpp
)

target_include_directories(live_share_controller_test PRIVATE
    ../../app/libs/live_share_controller
)
target_compile_features(live_share_controller_test PRIVATE cxx_std_11)
target_compile_options(live_share_controller_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME live_share_controller_test COMMAND live_share_controller_test)
