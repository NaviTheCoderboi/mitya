cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME "mitya")
project(${PROJECT_NAME} C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(mitya)
add_library(mitya::mitya ALIAS mitya)

target_sources(${PROJECT_NAME} PRIVATE
    src/image.c
)
target_include_directories(mitya
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Wpedantic>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>
)

option(MITYA_BUILD_EXAMPLES "Build examples" ON)

if(MITYA_BUILD_EXAMPLES)
    add_executable(chessboard examples/chessboard.cpp)
    target_link_libraries(chessboard PRIVATE mitya)

    add_executable(empty examples/empty.cpp)
    target_link_libraries(empty PRIVATE mitya)
endif()

option(MITYA_ENABLE_PNG "Enable PNG codec" OFF)

if(MITYA_ENABLE_PNG)
    find_package(PNG REQUIRED)
    target_sources(${PROJECT_NAME} PRIVATE src/codec_png.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MITYA_ENABLE_PNG)
    target_link_libraries(${PROJECT_NAME} PRIVATE PNG::PNG m)
endif()

option(MITYA_ENABLE_PNM "Enable PNM codec" OFF)
if(MITYA_ENABLE_PNM)
    target_sources(${PROJECT_NAME} PRIVATE src/codec_pnm.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MITYA_ENABLE_PNM)
endif()