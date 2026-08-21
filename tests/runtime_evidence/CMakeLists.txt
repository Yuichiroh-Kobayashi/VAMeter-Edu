cmake_minimum_required(VERSION 3.16)
project(runtime_evidence_test LANGUAGES CXX)

enable_testing()

add_executable(runtime_evidence_test
    runtime_evidence_test.cpp
    ../../app/libs/runtime_evidence/runtime_evidence.cpp
)

target_include_directories(runtime_evidence_test PRIVATE
    ../../app/libs/runtime_evidence
)

target_compile_features(runtime_evidence_test PRIVATE cxx_std_11)
target_compile_options(runtime_evidence_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME runtime_evidence_test COMMAND runtime_evidence_test)
