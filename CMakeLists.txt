cmake_minimum_required(VERSION 3.8)
project(MC_APP_BOILERPLATE VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

# Keep the diagnostic HTTPD stack explicit and overridable for out-of-tree
# A/B builds.  This is a STRING cache entry (not a boolean option) so 4096 and
# 8192 remain visible in generated configuration.
set(VAMETER_D2B_HTTPD_STACK_SIZE "4096" CACHE STRING "D2B HTTPD stack size in bytes (4096..16384)")
if(NOT "${VAMETER_D2B_HTTPD_STACK_SIZE}" MATCHES "^[0-9]+$")
    message(FATAL_ERROR "VAMETER_D2B_HTTPD_STACK_SIZE must be an integer in [4096,16384]")
endif()
set(_vameter_d2b_httpd_stack_size_value "${VAMETER_D2B_HTTPD_STACK_SIZE}")
math(EXPR _vameter_d2b_httpd_stack_size_value "${_vameter_d2b_httpd_stack_size_value}")
if(_vameter_d2b_httpd_stack_size_value LESS 4096 OR _vameter_d2b_httpd_stack_size_value GREATER 16384)
    message(FATAL_ERROR "VAMETER_D2B_HTTPD_STACK_SIZE must be an integer in [4096,16384]")
endif()
unset(_vameter_d2b_httpd_stack_size_value)

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
    add_subdirectory(tests/d2b_stop_failure_recovery)
    add_subdirectory(tests/runtime_evidence)
    add_subdirectory(tests/d2b_httpd_send_pump)
    add_subdirectory(tests/d2b_httpd_lifecycle)
    add_subdirectory(tests/d2b_ap_operation)
    add_subdirectory(tests/d2b_vi_transport)
    add_subdirectory(tests/d2b_vi_frame_writer)
    add_subdirectory(tests/d2b_vi_producer)
    add_subdirectory(tests/d2b_vi_backpressure)
    add_subdirectory(tests/reset_breadcrumb)
endif()
