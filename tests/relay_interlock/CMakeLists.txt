cmake_minimum_required(VERSION 3.16)
project(relay_interlock_test LANGUAGES CXX)
enable_testing()
add_executable(relay_interlock_test
    relay_interlock_test.cpp
    ../../app/libs/relay_interlock/relay_interlock.cpp
)
target_include_directories(relay_interlock_test PRIVATE ../../app/libs/relay_interlock)
target_compile_features(relay_interlock_test PRIVATE cxx_std_11)
target_compile_options(relay_interlock_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME relay_interlock_test COMMAND relay_interlock_test)
