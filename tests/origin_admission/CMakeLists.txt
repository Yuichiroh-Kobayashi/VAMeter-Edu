cmake_minimum_required(VERSION 3.16)
project(origin_admission_test LANGUAGES CXX)

option(ORIGIN_ADMISSION_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
enable_testing()

add_library(origin_admission STATIC ../../app/libs/origin_admission/origin_admission.cpp)
target_include_directories(origin_admission PUBLIC ../../app/libs/origin_admission)
target_compile_features(origin_admission PUBLIC cxx_std_11)
target_compile_options(origin_admission PRIVATE -Wall -Wextra -Wpedantic -Werror)

foreach(target origin_admission_test origin_admission_fuzz)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE origin_admission)
    target_compile_features(${target} PRIVATE cxx_std_11)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(ORIGIN_ADMISSION_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endforeach()

if(ORIGIN_ADMISSION_SANITIZERS)
    target_compile_options(origin_admission PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

add_test(NAME origin_admission_test COMMAND origin_admission_test)
add_test(NAME origin_admission_fuzz COMMAND origin_admission_fuzz)
