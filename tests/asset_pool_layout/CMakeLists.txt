add_executable(asset_pool_layout_test
    asset_pool_layout_test.cpp
    ../../app/libs/asset_pool_layout/asset_pool_layout.cpp
)
target_include_directories(asset_pool_layout_test PRIVATE ../../app)
set_target_properties(asset_pool_layout_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(asset_pool_layout_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME asset_pool_layout_test COMMAND asset_pool_layout_test)

if(PLATFORM_BUILD_DESKTOP)
    # Exercise the real app/AssetPool implementations; app_layer inherits C++17
    # from the existing desktop LovyanGFX target. The pure library test above is C++11.
    add_executable(asset_pool_host_driver asset_pool_host_driver.cpp)
    target_link_libraries(asset_pool_host_driver PRIVATE app_layer)
    add_test(NAME asset_pool_setup_failure_test COMMAND asset_pool_host_driver setup-failure)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME asset_pool_container_test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/container_test.py
            --driver $<TARGET_FILE:asset_pool_host_driver>
            --viewer-driver $<TARGET_FILE:viewer_asset_contract_test>
            --source-root ${PROJECT_SOURCE_DIR}
    )
endif()
