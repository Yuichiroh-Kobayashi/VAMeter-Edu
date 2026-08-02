cmake_minimum_required(VERSION 3.16)
project(web_server_owner_test LANGUAGES CXX)

enable_testing()

add_executable(web_server_owner_test
    web_server_owner_test.cpp
    ../../app/libs/web_server_owner/web_server_owner.cpp
    ../../app/libs/web_server_owner/web_server_results.cpp
)

target_include_directories(web_server_owner_test PRIVATE
    ../../app/libs/web_server_owner
)

target_compile_features(web_server_owner_test PRIVATE cxx_std_11)
target_compile_options(web_server_owner_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME web_server_owner_test COMMAND web_server_owner_test)
