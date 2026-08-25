cmake_minimum_required(VERSION 3.16)
project(signed_current_observation_test LANGUAGES CXX)

enable_testing()

add_executable(signed_current_observation_test
    signed_current_observation_test.cpp
    ../../app/libs/signed_current_observation/signed_current_observation.cpp
)

target_include_directories(signed_current_observation_test PRIVATE
    ../../app/libs/signed_current_observation
)

target_compile_features(signed_current_observation_test PRIVATE cxx_std_11)
target_compile_options(signed_current_observation_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME signed_current_observation_test COMMAND signed_current_observation_test)
