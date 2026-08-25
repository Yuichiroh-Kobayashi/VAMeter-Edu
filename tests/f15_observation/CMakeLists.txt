cmake_minimum_required(VERSION 3.16)
project(f15_observation_test LANGUAGES CXX)

enable_testing()

add_executable(f15_observation_test
    f15_observation_test.cpp
    ../../app/libs/f15_observation/f15_observation.cpp
)

target_include_directories(f15_observation_test PRIVATE
    ../../app/libs/f15_observation
)

target_compile_features(f15_observation_test PRIVATE cxx_std_11)
target_compile_options(f15_observation_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME f15_observation_test COMMAND f15_observation_test)
