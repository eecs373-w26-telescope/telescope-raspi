cmake_minimum_required(VERSION 3.20)
project(373-telescope LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)

# Auto-fetch raylib
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(raylib)

# Main executable
file(GLOB_RECURSE SOURCES src/*.cpp src/*.c src/*.h src/*.hpp)
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# Original Flip display logic
if(DEFINED DISPLAY_FLIP_OVERRIDE)
    set(DISPLAY_FLIP ${DISPLAY_FLIP_OVERRIDE})
    unset(DISPLAY_FLIP_OVERRIDE CACHE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(DISPLAY_FLIP 1)
else()
    set(DISPLAY_FLIP 0)
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE DISPLAY_FLIP=${DISPLAY_FLIP})

# Symlink resources to build directory
if(EXISTS ${CMAKE_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
    )
endif()
