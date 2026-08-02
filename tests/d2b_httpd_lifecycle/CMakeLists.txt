cmake_minimum_required(VERSION 3.16)
project(d2b_httpd_lifecycle_test LANGUAGES CXX)

enable_testing()

add_executable(d2b_httpd_lifecycle_test
    d2b_httpd_lifecycle_test.cpp
    ../../app/libs/d2b_vi/d2b_httpd_lifecycle_state.cpp
    ../../app/libs/d2b_vi/d2b_httpd_send_pump_state.cpp
)

target_include_directories(d2b_httpd_lifecycle_test PRIVATE
    ../../app/libs/d2b_vi
)

target_compile_features(d2b_httpd_lifecycle_test PRIVATE cxx_std_11)
target_compile_options(d2b_httpd_lifecycle_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME d2b_httpd_lifecycle_test COMMAND d2b_httpd_lifecycle_test)
