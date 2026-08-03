# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

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

add_compile_options(-Wno-format-truncation)

set(EXTRA_COMPONENT_DIRS
  ../../dependencies
  ${CMAKE_SOURCE_DIR}/../../app/libs/measurement_pipeline
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vameter_user_demo)
