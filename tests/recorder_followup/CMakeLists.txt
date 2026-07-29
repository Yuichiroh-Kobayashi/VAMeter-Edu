cmake_minimum_required(VERSION 3.16)
project(recorder_followup_test LANGUAGES CXX)

enable_testing()

add_executable(recorder_followup_test
    recorder_followup_test.cpp
    ../../app/libs/record_csv/record_csv.cpp
    ../../app/libs/waveform_scale/waveform_scale.cpp
)

target_include_directories(recorder_followup_test PRIVATE
    ../../app/libs/record_csv
    ../../app/libs/recorder_lifecycle
    ../../app/libs/waveform_scale
)

target_compile_features(recorder_followup_test PRIVATE cxx_std_11)
target_compile_options(recorder_followup_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME recorder_followup_test COMMAND recorder_followup_test)
