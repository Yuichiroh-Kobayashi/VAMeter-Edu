cmake_minimum_required(VERSION 3.16)
project(educational_ui_rev2_test LANGUAGES CXX)

enable_testing()

add_executable(educational_ui_rev2_test
    educational_ui_rev2_test.cpp
    ../../app/libs/live_share_controller/live_share_controller.cpp
    ../../app/libs/live_share_session/live_share_session.cpp
    ../../app/libs/meter_help_qr/meter_help_qr_urls.cpp
    ../../app/apps/utils/qrcode/qrcodegen/qrcodegen.cpp
)

target_include_directories(educational_ui_rev2_test PRIVATE
    ../../app
    ../../app/libs/live_share_controller
    ../../app/libs/live_share_session
    ../../app/libs/meter_help_qr
    ../../app/libs/web_server_owner
    ../../app/apps/utils/qrcode/qrcodegen
)

target_compile_features(educational_ui_rev2_test PRIVATE cxx_std_11)
target_compile_options(educational_ui_rev2_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME educational_ui_rev2_test COMMAND educational_ui_rev2_test)
