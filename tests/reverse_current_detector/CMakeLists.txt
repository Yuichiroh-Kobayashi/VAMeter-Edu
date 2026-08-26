cmake_minimum_required(VERSION 3.16)
project(reverse_current_detector_test LANGUAGES CXX)
enable_testing()
add_executable(reverse_current_detector_test
    reverse_current_detector_test.cpp
    ../../app/libs/reverse_current_detector/reverse_current_detector.cpp
)
target_include_directories(reverse_current_detector_test PRIVATE ../../app/libs/reverse_current_detector)
target_compile_features(reverse_current_detector_test PRIVATE cxx_std_11)
target_compile_options(reverse_current_detector_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME reverse_current_detector_test COMMAND reverse_current_detector_test)
