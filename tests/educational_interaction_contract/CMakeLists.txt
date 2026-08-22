cmake_minimum_required(VERSION 3.16)
project(educational_interaction_contract_test LANGUAGES CXX)

enable_testing()

add_executable(educational_interaction_contract_test
    educational_interaction_contract_test.cpp
    ../../app/libs/live_share_controller/live_share_controller.cpp
    ../../app/libs/live_share_session/live_share_session.cpp
    ../../app/libs/meter_help_qr/meter_help_qr_urls.cpp
    ../../app/apps/utils/qrcode/qrcodegen/qrcodegen.cpp
)

target_include_directories(educational_interaction_contract_test PRIVATE
    ../../app
    ../../app/libs/live_share_controller
    ../../app/libs/live_share_session
    ../../app/libs/meter_help_qr
    ../../app/libs/web_server_owner
    ../../app/apps/utils/qrcode/qrcodegen
)

target_compile_features(educational_interaction_contract_test PRIVATE cxx_std_11)
target_compile_options(educational_interaction_contract_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME educational_interaction_contract_test COMMAND educational_interaction_contract_test)
