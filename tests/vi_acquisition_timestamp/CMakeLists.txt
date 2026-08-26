cmake_minimum_required(VERSION 3.16)
project(vi_acquisition_timestamp_test LANGUAGES CXX)

enable_testing()

add_executable(vi_acquisition_timestamp_test
    vi_acquisition_timestamp_test.cpp
)

target_include_directories(vi_acquisition_timestamp_test PRIVATE
    ../../app/libs/d2b_vi
)

target_compile_features(vi_acquisition_timestamp_test PRIVATE cxx_std_11)
target_compile_options(vi_acquisition_timestamp_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME vi_acquisition_timestamp_test COMMAND vi_acquisition_timestamp_test)
