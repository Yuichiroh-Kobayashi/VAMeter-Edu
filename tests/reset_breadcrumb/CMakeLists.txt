cmake_minimum_required(VERSION 3.16)
project(reset_breadcrumb_test LANGUAGES CXX)

enable_testing()

add_executable(reset_breadcrumb_test
    reset_breadcrumb_test.cpp
    ../../app/libs/runtime_evidence/reset_breadcrumb.cpp
    ../../app/libs/runtime_evidence/runtime_evidence.cpp
)

target_include_directories(reset_breadcrumb_test PRIVATE
    ../../app/libs/runtime_evidence
)

target_compile_features(reset_breadcrumb_test PRIVATE cxx_std_11)
target_compile_options(reset_breadcrumb_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME reset_breadcrumb COMMAND reset_breadcrumb_test)
add_test(NAME httpd_stack COMMAND reset_breadcrumb_test)
add_test(NAME httpd_stack_config
    COMMAND ${CMAKE_COMMAND}
        -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stack_config_test.cmake
)
