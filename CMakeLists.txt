cmake_minimum_required(VERSION 3.25)

project(dab_receiver VERSION 0.1.0 LANGUAGES CXX)

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
