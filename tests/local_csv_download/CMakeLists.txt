cmake_minimum_required(VERSION 3.16)
project(local_csv_download_test LANGUAGES CXX)

enable_testing()

add_executable(local_csv_download_test
    local_csv_download_test.cpp
    ../../app/libs/local_csv_download/local_csv_download_name.cpp
    ../../app/libs/local_csv_download/local_csv_download_selection.cpp
    ../../app/libs/local_csv_download/local_csv_stream.cpp
)

target_include_directories(local_csv_download_test PRIVATE
    ../../app/libs/local_csv_download
)

target_compile_features(local_csv_download_test PRIVATE cxx_std_11)
target_compile_options(local_csv_download_test PRIVATE -Wall -Wextra -Werror)

add_test(NAME local_csv_download_test COMMAND local_csv_download_test)
