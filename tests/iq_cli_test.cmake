file(MAKE_DIRECTORY "${TEST_DIR}")
file(WRITE "${TEST_DIR}/empty.iq" "")
file(WRITE "${TEST_DIR}/malformed.iq" "x")

function(check_exit expected)
    execute_process(COMMAND "${RECEIVER}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL expected)
        message(FATAL_ERROR "Expected exit ${expected}, got ${result}: ${output} ${error}")
    endif()
    if(expected EQUAL 1 AND NOT error MATCHES "Error:|Usage:")
        message(FATAL_ERROR "Missing helpful error: ${error}")
    endif()
endfunction()

# An empty recording is valid for the summary-only command.
check_exit(0 "${TEST_DIR}/empty.iq" 2048000)
# Both positional arguments are required.
check_exit(1)
# Reject zero, negative, fractional, nonnumeric, and overflowing sample rates.
foreach(rate 0 -1 1.5 abc 18446744073709551616)
    check_exit(1 "${TEST_DIR}/empty.iq" "${rate}")
endforeach()
# Unknown output options must fail with usage guidance.
check_exit(1 "${TEST_DIR}/empty.iq" 1000 --unknown output.csv)
# An output option needs a destination path.
check_exit(1 "${TEST_DIR}/empty.iq" 1000 --power-csv)
# An output option cannot be repeated.
check_exit(1 "${TEST_DIR}/empty.iq" 1000 --power-csv a --power-csv b)
# Another option cannot stand in for a missing path.
check_exit(1 "${TEST_DIR}/empty.iq" 1000 --power-csv --spectrum-csv b)
# Opening a nonexistent IQ input must fail.
check_exit(1 "${TEST_DIR}/missing.iq" 1000)
# An unmatched final I byte must reject the export.
check_exit(1 "${TEST_DIR}/malformed.iq" 1000 --power-csv "${TEST_DIR}/failed.csv")
# A requested spectrum needs at least 2048 complex samples.
check_exit(1 "${TEST_DIR}/empty.iq" 1000 --spectrum-csv "${TEST_DIR}/failed.csv")
# The output directory must already exist.
check_exit(1 "${TEST_DIR}/empty.iq" 1000 --power-csv "${TEST_DIR}/missing/output.csv")
# Failed exports must not publish a final CSV.
if(EXISTS "${TEST_DIR}/failed.csv")
    message(FATAL_ERROR "A failed export published a CSV")
endif()
