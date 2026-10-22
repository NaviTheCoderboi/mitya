cmake_minimum_required(VERSION 3.21)

project(mitya LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MITYA_ENABLE_PNG "Enable PNG codec" OFF)
option(MITYA_ENABLE_PNM "Enable PNM codec" OFF)
option(MITYA_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})

set(MITYA_USER_CODECS_HEADER "" CACHE STRING
    "Header that defines MITYA_USER_CODECS")

add_library(mitya)
add_library(mitya::mitya ALIAS mitya)

target_sources(mitya PRIVATE
    src/image.c
    # src/ops.c
)

target_include_directories(mitya
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(mitya PRIVATE c_std_23)
set_target_properties(mitya PROPERTIES C_EXTENSIONS OFF)

if(NOT MSVC)
    target_compile_options(mitya PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MITYA_USER_CODECS_HEADER)
    target_compile_definitions(mitya PUBLIC
        MITYA_USER_CODECS_HEADER="${MITYA_USER_CODECS_HEADER}")
endif()

if(MITYA_ENABLE_PNG)
    find_package(PNG REQUIRED)
    target_sources(mitya PRIVATE src/codec_png.c)
    target_compile_definitions(mitya PRIVATE MITYA_ENABLE_PNG)
    target_link_libraries(mitya PRIVATE PNG::PNG $<$<BOOL:${UNIX}>:m>)
endif()

if(MITYA_ENABLE_PNM)
    target_sources(mitya PRIVATE src/codec_pnm.c)
    target_compile_definitions(mitya PRIVATE MITYA_ENABLE_PNM)
endif()

if(MITYA_BUILD_EXAMPLES)
    enable_language(CXX)

    foreach(ex chessboard empty)
        add_executable(${ex} examples/${ex}.cpp)
        target_link_libraries(${ex} PRIVATE mitya)
        target_compile_features(${ex} PRIVATE cxx_std_23)
        set_target_properties(${ex} PROPERTIES CXX_EXTENSIONS OFF)

        if(NOT MSVC)
            target_compile_options(${ex} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
endif()