cmake_minimum_required(VERSION 3.16)
project(d2b_vi_transport_test LANGUAGES CXX)

enable_testing()

add_executable(d2b_vi_transport_test
    d2b_vi_transport_test.cpp
    ../../app/libs/d2b_vi/d2b_control.cpp
    ../../app/libs/d2b_vi/d2b_message_buffer.cpp
    ../../app/libs/d2b_vi/d2b_session.cpp
)

target_include_directories(d2b_vi_transport_test PRIVATE
    ../../app/libs/d2b_vi
)

target_compile_features(d2b_vi_transport_test PRIVATE cxx_std_11)
target_compile_options(d2b_vi_transport_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME d2b_vi_transport_test COMMAND d2b_vi_transport_test)

add_executable(d2b_control_cli
    d2b_control_cli.cpp
    ../../app/libs/d2b_vi/d2b_control.cpp
)

target_include_directories(d2b_control_cli PRIVATE
    ../../app/libs/d2b_vi
)

target_compile_features(d2b_control_cli PRIVATE cxx_std_11)
target_compile_options(d2b_control_cli PRIVATE -Wall -Wextra -Werror)

add_executable(d2b_capabilities_cli
    d2b_capabilities_cli.cpp
    ../../app/libs/d2b_vi/d2b_capabilities.cpp
)

target_include_directories(d2b_capabilities_cli PRIVATE
    ../../app/libs/d2b_vi
)

target_compile_features(d2b_capabilities_cli PRIVATE cxx_std_11)
target_compile_options(d2b_capabilities_cli PRIVATE -Wall -Wextra -Werror)
