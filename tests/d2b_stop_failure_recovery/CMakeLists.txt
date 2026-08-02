cmake_minimum_required(VERSION 3.16)
project(d2b_stop_failure_recovery_test LANGUAGES CXX)

enable_testing()

add_executable(d2b_stop_failure_recovery_test
    d2b_stop_failure_recovery_test.cpp
    ../../app/libs/web_server_owner/web_server_owner.cpp
    ../../app/libs/web_server_owner/web_server_results.cpp
    ../../app/libs/web_server_owner/web_server_transaction.cpp
    ../../app/apps/app_settings/view/network_web_server_recovery.cpp
    ../../app/libs/d2b_vi/d2b_stream_start_disposition.cpp
    ../../app/libs/d2b_vi/d2b_transport_runtime_state.cpp
)

target_include_directories(d2b_stop_failure_recovery_test PRIVATE
    ../../app/libs/web_server_owner
    ../../app/libs/d2b_vi
    ../../app/apps/app_settings/view
)

target_compile_features(d2b_stop_failure_recovery_test PRIVATE cxx_std_11)
target_compile_options(d2b_stop_failure_recovery_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME d2b_stop_failure_recovery_test COMMAND d2b_stop_failure_recovery_test)
