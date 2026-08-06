cmake_minimum_required(VERSION 3.16)
project(httpd_stack_diag_test LANGUAGES CXX)

add_executable(httpd_stack_diag_test
    httpd_stack_diag_test.cpp
    ../../app/libs/httpd_stack_diag/httpd_stack_diag.cpp
)

target_include_directories(httpd_stack_diag_test PRIVATE ../../app/libs/httpd_stack_diag)
target_compile_features(httpd_stack_diag_test PRIVATE cxx_std_11)
target_compile_options(httpd_stack_diag_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME httpd_stack_diag_test COMMAND httpd_stack_diag_test)
