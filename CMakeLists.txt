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
    add_subdirectory(tests/local_csv_download)
    add_subdirectory(tests/recorder_followup)
    add_subdirectory(tests/web_server_owner)
    add_subdirectory(tests/d2b_vi_transport)
    add_subdirectory(tests/d2b_vi_frame_writer)
    add_subdirectory(tests/d2b_vi_producer)
    add_subdirectory(tests/d2b_vi_backpressure)
endif()
