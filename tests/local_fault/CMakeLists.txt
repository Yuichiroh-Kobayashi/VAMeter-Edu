cmake_minimum_required(VERSION 3.16)
project(local_fault_test LANGUAGES CXX)

enable_testing()

add_executable(local_fault_test
    local_fault_test.cpp
    ../../app/libs/local_fault/local_fault.cpp
)

target_include_directories(local_fault_test PRIVATE
    ../../app/libs/local_fault
)
target_compile_features(local_fault_test PRIVATE cxx_std_11)
target_compile_options(local_fault_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME local_fault_test COMMAND local_fault_test)
