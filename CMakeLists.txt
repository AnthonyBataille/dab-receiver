cmake_minimum_required(VERSION 3.25)

project(dab_receiver VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_executable(dab_receiver src/main.cpp)

add_library(dab_iq_reader src/iq_file_reader.cpp)
target_include_directories(dab_iq_reader PUBLIC src)
target_compile_features(dab_iq_reader PUBLIC cxx_std_20)
set_target_properties(dab_iq_reader PROPERTIES CXX_EXTENSIONS OFF)

add_library(dab_iq_summary src/iq_diagnostics.cpp
    src/offline_iq_runner.cpp)
target_include_directories(dab_iq_summary PUBLIC src)
target_link_libraries(dab_iq_summary PUBLIC dab_iq_reader dab_null_symbol_detector)
target_compile_features(dab_iq_summary PUBLIC cxx_std_20)
set_target_properties(dab_iq_summary PROPERTIES CXX_EXTENSIONS OFF)

add_library(dab_iq_output src/iq_output.cpp)
target_link_libraries(dab_iq_output PUBLIC dab_iq_summary)
set_target_properties(dab_iq_output PROPERTIES CXX_EXTENSIONS OFF)

add_library(dab_null_symbol_detector src/null_symbol_detector.cpp)
target_include_directories(dab_null_symbol_detector PUBLIC src)
target_compile_features(dab_null_symbol_detector PUBLIC cxx_std_20)
set_target_properties(dab_null_symbol_detector PROPERTIES CXX_EXTENSIONS OFF)

target_compile_features(dab_receiver PRIVATE cxx_std_20)
target_link_libraries(dab_receiver PRIVATE dab_iq_reader dab_iq_summary dab_iq_output)
set_target_properties(dab_receiver PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(dab_iq_reader PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_options(dab_iq_output PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_options(dab_iq_summary PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_options(
        dab_receiver
        PRIVATE
            /W4
            /WX
            /permissive-
            /utf-8
            /Zc:__cplusplus
    )
endif()

if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(dab_null_symbol_detector_test tests/null_symbol_detector_test.cpp)
    target_link_libraries(dab_null_symbol_detector_test PRIVATE dab_null_symbol_detector Catch2::Catch2WithMain)
    if(MSVC)
        target_compile_options(dab_null_symbol_detector_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME null_symbol_detector_test COMMAND dab_null_symbol_detector_test)

    add_executable(dab_iq_reader_test tests/iq_file_reader_test.cpp)
    target_link_libraries(dab_iq_reader_test PRIVATE dab_iq_reader Catch2::Catch2WithMain)
    if(MSVC)
        target_compile_options(dab_iq_reader_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME iq_reader_test COMMAND dab_iq_reader_test)

    add_executable(dab_iq_diagnostics_test tests/iq_diagnostics_test.cpp)
    target_link_libraries(dab_iq_diagnostics_test PRIVATE dab_iq_summary dab_iq_output Catch2::Catch2WithMain)
    if(MSVC)
        target_compile_options(dab_iq_diagnostics_test PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME iq_diagnostics_test COMMAND dab_iq_diagnostics_test)
    add_test(NAME iq_cli_test COMMAND ${CMAKE_COMMAND}
        -DRECEIVER=$<TARGET_FILE:dab_receiver>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/iq_cli_test.cmake)
endif()
