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

set(SRC_DIR src)
set(INCLUDE_DIR include)

add_executable(${PROJECT_NAME} ${SRC_DIR}/main.cpp)

file(GLOB_RECURSE SRC_FILES ${SRC_DIR}/*.c ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE HEADER_FILES ${INCLUDE_DIR}/*.h ${INCLUDE_DIR}/*.hpp)

target_sources(${PROJECT_NAME} PRIVATE
    ${SRC_FILES}
    ${HEADER_FILES}
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SRC_DIR}
    ${INCLUDE_DIR}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Wpedantic;-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-fno-exceptions>
)

option(MITYA_ENABLE_PNG "Enable PNG codec" ON)

if(MITYA_ENABLE_PNG)
    find_package(PNG REQUIRED)
    target_sources(${PROJECT_NAME} PRIVATE src/codec_png.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MITYA_ENABLE_PNG)
    target_link_libraries(${PROJECT_NAME} PRIVATE PNG::PNG m)
endif()

option(MITYA_ENABLE_PNM "Enable PNM codec" ON)
if(MITYA_ENABLE_PNM)
    target_sources(${PROJECT_NAME} PRIVATE src/codec_pnm.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MITYA_ENABLE_PNM)
endif()