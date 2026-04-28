cmake_minimum_required(VERSION 3.16)
project(motor_observe_state_test LANGUAGES CXX)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_executable(motor_observe_state_test
    motor_observe_state_test.cpp
    "${REPO_ROOT}/app/libs/motor_observe_safety/motor_observe_safety.cpp"
    "${REPO_ROOT}/app/libs/motor_observe_backend/motor_observe_backend.cpp"
)

target_include_directories(motor_observe_state_test PRIVATE
    "${REPO_ROOT}/app"
)

target_compile_features(motor_observe_state_test PRIVATE cxx_std_11)
