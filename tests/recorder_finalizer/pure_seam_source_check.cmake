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
