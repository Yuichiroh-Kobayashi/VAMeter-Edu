cmake_minimum_required(VERSION 3.16)
project(d2b_vi_frame_writer_test LANGUAGES CXX)

enable_testing()

add_executable(d2b_vi_frame_writer_test
    d2b_vi_frame_writer_test.cpp
    ../../app/libs/d2b_vi/d2b_frame_writer.cpp
)

target_include_directories(d2b_vi_frame_writer_test PRIVATE
    ../../app/libs/d2b_vi
)

target_compile_features(d2b_vi_frame_writer_test PRIVATE cxx_std_11)
target_compile_options(d2b_vi_frame_writer_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME d2b_vi_frame_writer_test COMMAND d2b_vi_frame_writer_test)

add_executable(d2b_vi_frame_cli
    d2b_vi_frame_cli.cpp
    ../../app/libs/d2b_vi/d2b_frame_writer.cpp
)

target_include_directories(d2b_vi_frame_cli PRIVATE
    ../../app/libs/d2b_vi
)

target_compile_features(d2b_vi_frame_cli PRIVATE cxx_std_11)
target_compile_options(d2b_vi_frame_cli PRIVATE -Wall -Wextra -Werror)
