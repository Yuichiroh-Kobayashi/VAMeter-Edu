cmake_minimum_required(VERSION 3.8)
project(MC_APP_BOILERPLATE VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

# Set C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform 
option(PLATFORM_BUILD_DESKTOP "Build for platform desktop" ON)

if(PLATFORM_BUILD_DESKTOP)
    include(platforms/desktop/CMakeLists.txt)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests/live_share_session)
    add_subdirectory(tests/live_share_controller)
    add_subdirectory(tests/local_fault)
    add_subdirectory(tests/educational_interaction_contract)
    add_subdirectory(tests/local_csv_download)
    add_subdirectory(tests/recorder_followup)
    add_subdirectory(tests/web_server_owner)
    add_subdirectory(tests/d2b_stop_failure_recovery)
    add_subdirectory(tests/runtime_evidence)
    add_subdirectory(tests/d2b_httpd_send_pump)
    add_subdirectory(tests/d2b_httpd_lifecycle)
    add_subdirectory(tests/d2b_ap_operation)
    add_subdirectory(tests/d2b_vi_transport)
    add_subdirectory(tests/d2b_vi_frame_writer)
    add_subdirectory(tests/d2b_vi_producer)
    add_subdirectory(tests/d2b_vi_backpressure)
    add_subdirectory(tests/signed_current_observation)
    add_subdirectory(tests/viewer_asset_contract)
endif()
