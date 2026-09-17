cmake_minimum_required(VERSION 3.16)
project(reverse_current_safety_test LANGUAGES CXX)
enable_testing()
add_executable(reverse_current_safety_test
    reverse_current_safety_test.cpp
    ../../app/libs/reverse_current_detector/reverse_current_detector.cpp
    ../../app/libs/reverse_current_safety/reverse_current_safety.cpp
)
target_include_directories(reverse_current_safety_test PRIVATE
    ../../app/libs/reverse_current_detector
    ../../app/libs/reverse_current_safety
)
target_compile_features(reverse_current_safety_test PRIVATE cxx_std_11)
target_compile_options(reverse_current_safety_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME reverse_current_safety_test COMMAND reverse_current_safety_test)
