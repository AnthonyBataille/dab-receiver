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
    set(last_output "${output}" PARENT_SCOPE)
endfunction()

# An empty recording is valid for the summary-only command.
check_exit(0 "${TEST_DIR}/empty.iq" 2048000)
if(NOT last_output MATCHES "Complex samples: 0" OR NOT last_output MATCHES "Duration: 0.000 s")
    message(FATAL_ERROR "Missing summary without CSV options: ${last_output}")
endif()
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
# The exporter creates a missing output directory.
check_exit(0 "${TEST_DIR}/empty.iq" 1000 --power-csv "${TEST_DIR}/missing/output.csv")
# Failed exports must not publish a final CSV.
if(EXISTS "${TEST_DIR}/failed.csv")
    message(FATAL_ERROR "A failed export published a CSV")
endif()

# Assert contents, not just exit codes: single-output and empty-export regressions return success.
if(NOT last_output MATCHES "Complex samples: 0" OR NOT last_output MATCHES "Duration: 0.000 s")
    message(FATAL_ERROR "Missing empty-input summary: ${last_output}")
endif()
file(READ "${TEST_DIR}/missing/output.csv" empty_power)
string(REPLACE "\r\n" "\n" empty_power "${empty_power}")
if(NOT empty_power STREQUAL "window_start_seconds,mean_linear_power,sample_count\n")
    message(FATAL_ERROR "Empty power export must contain exactly its header")
endif()

function(check_csv path header expected_lines)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing CSV: ${path}")
    endif()
    file(READ "${path}" csv)
    string(REPLACE "\r\n" "\n" csv "${csv}")
    string(FIND "${csv}" "${header}\n" header_position)
    string(REGEX MATCHALL "[^\n]+" lines "${csv}")
    list(LENGTH lines line_count)
    if(NOT header_position EQUAL 0 OR NOT line_count EQUAL expected_lines)
        message(FATAL_ERROR "Unexpected CSV header or row count: ${path} (${line_count})")
    endif()
endfunction()

# 2048 DC samples with I = Q = -0.5: power is 0.5 and duration is one second.
string(REPEAT "@" 4096 iq_bytes)
file(WRITE "${TEST_DIR}/tone.iq" "${iq_bytes}")
check_exit(0 "${TEST_DIR}/tone.iq" 2048 --power-csv "${TEST_DIR}/power-only.csv")
check_csv("${TEST_DIR}/power-only.csv" "window_start_seconds,mean_linear_power,sample_count" 5)
file(READ "${TEST_DIR}/power-only.csv" power)
if(NOT power MATCHES "0,0.5,512")
    message(FATAL_ERROR "Incorrect power for the DC fixture")
endif()
if(NOT last_output MATCHES "Complex samples: 2048" OR NOT last_output MATCHES "Duration: 1.000 s")
    message(FATAL_ERROR "Incorrect recording summary: ${last_output}")
endif()
check_exit(0 "${TEST_DIR}/tone.iq" 2048 --spectrum-csv "${TEST_DIR}/spectrum-only.csv")
check_csv("${TEST_DIR}/spectrum-only.csv" "relative_frequency_hz,linear_bin_power" 2049)
file(READ "${TEST_DIR}/spectrum-only.csv" spectrum)
if(NOT spectrum MATCHES "\n0,0.5[\r\n]")
    message(FATAL_ERROR "Incorrect DC spectrum power")
endif()
check_exit(0 "${TEST_DIR}/tone.iq" 2048 --power-csv "${TEST_DIR}/power.csv"
    --spectrum-csv "${TEST_DIR}/spectrum.csv")
check_csv("${TEST_DIR}/power.csv" "window_start_seconds,mean_linear_power,sample_count" 5)
check_csv("${TEST_DIR}/spectrum.csv" "relative_frequency_hz,linear_bin_power" 2049)
file(READ "${TEST_DIR}/power.csv" old_power)
file(READ "${TEST_DIR}/spectrum.csv" old_spectrum)

# A malformed replacement must preserve both old outputs.
check_exit(1 "${TEST_DIR}/malformed.iq" 2048 --power-csv "${TEST_DIR}/power.csv"
    --spectrum-csv "${TEST_DIR}/spectrum.csv")
file(READ "${TEST_DIR}/power.csv" preserved_power)
file(READ "${TEST_DIR}/spectrum.csv" preserved_spectrum)
if(NOT preserved_power STREQUAL old_power OR NOT preserved_spectrum STREQUAL old_spectrum)
    message(FATAL_ERROR "Failed processing changed existing CSVs")
endif()

# Destination validation now belongs to the CLI, rather than the diagnostics sink.
file(WRITE "${TEST_DIR}/shared.csv" "previous contents")
check_exit(1 "${TEST_DIR}/tone.iq" 2048 --power-csv "${TEST_DIR}/shared.csv"
    --spectrum-csv "${TEST_DIR}/shared.csv")
file(READ "${TEST_DIR}/shared.csv" shared)
if(NOT shared STREQUAL "previous contents")
    message(FATAL_ERROR "Rejected duplicate destination was modified")
endif()
foreach(option --power-csv --spectrum-csv)
    check_exit(1 "${TEST_DIR}/tone.iq" 2048 ${option} "${TEST_DIR}/tone.iq")
    file(READ "${TEST_DIR}/tone.iq" preserved_input)
    if(NOT preserved_input STREQUAL iq_bytes)
        message(FATAL_ERROR "Rejected input destination was modified")
    endif()
endforeach()

# A subsequent successful run must replace both outputs with actual new data.
string(REPEAT " " 4096 replacement_bytes)
file(WRITE "${TEST_DIR}/tone.iq" "${replacement_bytes}")
check_exit(0 "${TEST_DIR}/tone.iq" 2048 --power-csv "${TEST_DIR}/power.csv"
    --spectrum-csv "${TEST_DIR}/spectrum.csv")
file(READ "${TEST_DIR}/power.csv" new_power)
file(READ "${TEST_DIR}/spectrum.csv" new_spectrum)
if(new_power STREQUAL old_power OR new_spectrum STREQUAL old_spectrum)
    message(FATAL_ERROR "Successful replacement did not update both CSVs")
endif()
file(GLOB staging_files "${TEST_DIR}/*.tmp-*" "${TEST_DIR}/missing/*.tmp-*")
if(staging_files)
    message(FATAL_ERROR "Exports left staging or backup files: ${staging_files}")
endif()
