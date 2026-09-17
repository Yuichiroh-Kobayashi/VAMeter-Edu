set(PURE_SEAM_FILES
    "${SOURCE_ROOT}/app/libs/recorder_sample_buffer/recorder_sample_buffer.h"
    "${SOURCE_ROOT}/app/libs/recorder_sample_buffer/recorder_sample_buffer.cpp"
    "${SOURCE_ROOT}/app/libs/recorder_finalizer/recorder_finalizer.h"
    "${SOURCE_ROOT}/app/libs/recorder_finalizer/recorder_finalizer.cpp"
)

foreach(SOURCE_FILE IN LISTS PURE_SEAM_FILES)
    file(READ "${SOURCE_FILE}" SOURCE_TEXT)
    foreach(FORBIDDEN_TEXT "std::vector" "new " "new(" "malloc" "calloc" "realloc" "make_unique" "make_shared")
        string(FIND "${SOURCE_TEXT}" "${FORBIDDEN_TEXT}" FOUND_AT)
        if(NOT FOUND_AT EQUAL -1)
            message(FATAL_ERROR "${SOURCE_FILE} contains forbidden allocation text: ${FORBIDDEN_TEXT}")
        endif()
    endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/app/libs/record_csv/record_csv.cpp" CSV_SOURCE)
foreach(REQUIRED_WRITER_CALL "std::fputs(Header(), file)" "std::fprintf(file, kVoltageSampleFormat" "std::fprintf(file, kCurrentSampleFormat" "std::fprintf(file, kBothSampleFormat")
    string(FIND "${CSV_SOURCE}" "${REQUIRED_WRITER_CALL}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "legacy CSV writer path is missing: ${REQUIRED_WRITER_CALL}")
    endif()
endforeach()

file(READ "${SOURCE_ROOT}/app/libs/recorder_finalizer/recorder_finalizer.h" FINALIZER_HEADER)
foreach(REQUIRED_APP_INCLUDE "libs/record_csv/record_csv.h" "libs/recorder_sample_buffer/recorder_sample_buffer.h")
    string(FIND "${FINALIZER_HEADER}" "${REQUIRED_APP_INCLUDE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "finalizer header is not closed from the app include root: ${REQUIRED_APP_INCLUDE}")
    endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/recorder_finalizer/CMakeLists.txt" FINALIZER_TEST_CMAKE)
string(FIND
    "${FINALIZER_TEST_CMAKE}"
    "target_include_directories(recorder_finalizer_test PRIVATE ../../app)"
    APP_ROOT_INCLUDE_AT
)
if(APP_ROOT_INCLUDE_AT EQUAL -1)
    message(FATAL_ERROR "host finalizer test must compile with only the app include root")
endif()
