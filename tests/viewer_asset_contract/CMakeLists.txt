cmake_minimum_required(VERSION 3.16)
project(viewer_asset_contract_test LANGUAGES CXX)

enable_testing()

add_executable(viewer_asset_contract_test
    viewer_asset_contract_test.cpp
    ../../app/libs/viewer_asset_contract/viewer_asset_contract.cpp
)

target_include_directories(viewer_asset_contract_test PRIVATE
    ../../app/libs/viewer_asset_contract
)
target_compile_features(viewer_asset_contract_test PRIVATE cxx_std_11)
target_compile_options(viewer_asset_contract_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME viewer_asset_contract_test COMMAND viewer_asset_contract_test)
