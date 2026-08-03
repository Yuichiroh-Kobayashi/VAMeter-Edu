if(NOT DEFINED ROOT_SOURCE_DIR)
    message(FATAL_ERROR "ROOT_SOURCE_DIR is required")
endif()

function(run_case label value expected_cache expected_success)
    set(binary_dir "/tmp/vameter-d2b-stack-config-${label}")
    file(REMOVE_RECURSE "${binary_dir}")
    set(arguments
        -S "${ROOT_SOURCE_DIR}"
        -B "${binary_dir}"
        -DPLATFORM_BUILD_DESKTOP=OFF
        -DBUILD_TESTING=OFF
    )
    if(NOT "${value}" STREQUAL "<default>")
        list(APPEND arguments "-DVAMETER_D2B_HTTPD_STACK_SIZE=${value}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${arguments}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
    if("${expected_success}" STREQUAL "TRUE")
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${label} configure unexpectedly failed: ${output}${error_output}")
        endif()
        file(READ "${binary_dir}/CMakeCache.txt" cache_text)
        if(NOT cache_text MATCHES "VAMETER_D2B_HTTPD_STACK_SIZE:STRING=${expected_cache}")
            message(FATAL_ERROR "${label} cache did not retain ${expected_cache}: ${cache_text}")
        endif()
    else()
        if(result EQUAL 0)
            message(FATAL_ERROR "${label} configure unexpectedly succeeded")
        endif()
    endif()
endfunction()

run_case(default "<default>" 4096 TRUE)
run_case(explicit_4096 4096 4096 TRUE)
run_case(explicit_8192 8192 8192 TRUE)
run_case(zero 0 0 FALSE)
run_case(below_minimum 4095 4095 FALSE)
run_case(above_maximum 16385 16385 FALSE)
run_case(negative -1 -1 FALSE)
run_case(non_integer not_an_integer not_an_integer FALSE)

message(STATUS "PASS: D2B HTTPD stack configuration matrix")
