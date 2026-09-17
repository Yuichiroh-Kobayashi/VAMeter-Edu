cmake_minimum_required(VERSION 3.16)
project(recorder_finalizer_test LANGUAGES CXX)

add_executable(recorder_finalizer_test
    recorder_finalizer_test.cpp
    ../../app/libs/record_csv/record_csv.cpp
    ../../app/libs/recorder_finalizer/recorder_finalizer.cpp
)
target_include_directories(recorder_finalizer_test PRIVATE
    ../../app/libs/record_csv
    ../../app/libs/recorder_finalizer
    ../../app/libs/recorder_sample_buffer
)
target_compile_features(recorder_finalizer_test PRIVATE cxx_std_11)
target_compile_options(recorder_finalizer_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME recorder_finalizer_test COMMAND recorder_finalizer_test)
add_test(
    NAME recorder_pure_seam_source_check
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/pure_seam_source_check.cmake
)
