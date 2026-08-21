cmake_minimum_required(VERSION 3.16)
project(d2b_ap_operation_test LANGUAGES CXX)

enable_testing()

add_executable(d2b_ap_operation
    d2b_ap_operation_test.cpp
    ../../app/libs/web_server_owner/web_server_ap_operation.cpp
    ../../app/libs/web_server_owner/web_server_owner.cpp
    ../../app/libs/web_server_owner/web_server_results.cpp
    ../../app/libs/web_server_owner/web_server_transaction.cpp
    ../../app/apps/app_settings/view/network_web_server_recovery.cpp
)

target_include_directories(d2b_ap_operation PRIVATE
    ../../app/libs/web_server_owner
    ../../app/apps/app_settings/view
)

target_compile_features(d2b_ap_operation PRIVATE cxx_std_11)
target_compile_options(d2b_ap_operation PRIVATE -Wall -Wextra -Werror)

add_test(NAME d2b_ap_operation COMMAND d2b_ap_operation)
