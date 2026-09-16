cmake_minimum_required(VERSION 3.16)
project(live_share_qr_presentation_test LANGUAGES CXX)

enable_testing()

add_executable(live_share_qr_presentation_test
    live_share_qr_presentation_test.cpp
    ../../app/libs/live_share_qr_presentation/live_share_qr_presentation.cpp
)

target_include_directories(live_share_qr_presentation_test PRIVATE
    ../../app
    ../../app/libs/live_share_qr_presentation
    ../../app/libs/live_share_session
)
target_compile_features(live_share_qr_presentation_test PRIVATE cxx_std_11)
target_compile_options(live_share_qr_presentation_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME live_share_qr_presentation_test COMMAND live_share_qr_presentation_test)
