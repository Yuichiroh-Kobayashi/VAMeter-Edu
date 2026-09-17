cmake_minimum_required(VERSION 3.16)
project(recorder_sample_buffer_test LANGUAGES CXX)

add_executable(recorder_sample_buffer_test
    recorder_sample_buffer_test.cpp
    ../../app/libs/recorder_sample_buffer/recorder_sample_buffer.cpp
)
target_include_directories(recorder_sample_buffer_test PRIVATE ../../app/libs/recorder_sample_buffer)
target_compile_features(recorder_sample_buffer_test PRIVATE cxx_std_11)
target_compile_options(recorder_sample_buffer_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME recorder_sample_buffer_test COMMAND recorder_sample_buffer_test)
