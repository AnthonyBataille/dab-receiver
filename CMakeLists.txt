cmake_minimum_required(VERSION 3.25)

project(dab_receiver VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_executable(dab_receiver src/main.cpp)

target_compile_features(dab_receiver PRIVATE cxx_std_20)
set_target_properties(dab_receiver PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
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

    add_executable(dab_receiver_smoke_test tests/smoke_test.cpp)

    target_compile_features(dab_receiver_smoke_test PRIVATE cxx_std_20)
    set_target_properties(dab_receiver_smoke_test PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(
            dab_receiver_smoke_test
            PRIVATE
                /W4
                /WX
                /permissive-
                /utf-8
                /Zc:__cplusplus
        )
    endif()

    target_link_libraries(dab_receiver_smoke_test PRIVATE Catch2::Catch2WithMain)

    add_test(NAME smoke_test COMMAND dab_receiver_smoke_test)
endif()
