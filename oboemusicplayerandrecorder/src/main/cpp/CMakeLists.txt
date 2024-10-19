cmake_minimum_required(VERSION 3.22.1)

set (PARSELIB_DIR ../../../../parselib)
set (IOLIB_DIR ../../../../iolib)
project("oboemusicplayerandrecorder")

get_filename_component(SAMPLE_ROOT_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ABSOLUTE)
include_directories(third_party)
set (OBOE_DIR ${SAMPLE_ROOT_DIR}/oboe)

add_subdirectory(${OBOE_DIR} ./oboe-bin)

include_directories(
        ${OBOE_DIR}/include
        ${OBOE_DIR}/src/flowgraph
        ${CMAKE_CURRENT_LIST_DIR}
)

include(${PARSELIB_DIR}/src/main/cpp/CMakeLists.txt)
include(${IOLIB_DIR}/src/main/cpp/CMakeLists.txt)

include_directories(
        ${PARSELIB_DIR}/src/main/cpp
        ${IOLIB_DIR}/src/main/cpp)

add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        oboemusicplayerandrecorder.cpp
        LiveEffectEngine.cpp
    )

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Werror "$<$<CONFIG:RELEASE>:-Ofast>")

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        android
        log
        iolib

        parselib
        oboe
)
