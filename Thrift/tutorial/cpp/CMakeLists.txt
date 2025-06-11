cmake_minimum_required(VERSION 3.10)
project(ComposePost)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Allow user to specify Thrift location
set(THRIFT_ROOT "/usr/local" CACHE PATH "Thrift installation directory")

# Find Thrift headers and libraries
find_path(THRIFT_INCLUDE_DIR thrift/Thrift.h
    PATHS
    ${THRIFT_ROOT}/include
    /usr/local/include
    /usr/include
    NO_DEFAULT_PATH
)

find_library(THRIFT_LIBRARIES thrift
    PATHS
    ${THRIFT_ROOT}/lib
    ${THRIFT_ROOT}/lib64
    /usr/local/lib
    /usr/local/lib64
    NO_DEFAULT_PATH
)

if(NOT THRIFT_INCLUDE_DIR OR NOT THRIFT_LIBRARIES)
    message(FATAL_ERROR 
        "Thrift not found. Please specify THRIFT_ROOT:\n"
        "cmake -DTHRIFT_ROOT=/path/to/thrift/installation ..")
endif()

message(STATUS "Found Thrift include: ${THRIFT_INCLUDE_DIR}")
message(STATUS "Found Thrift library: ${THRIFT_LIBRARIES}")

# Use existing generated sources
set(GENERATED_SOURCES
    ${CMAKE_SOURCE_DIR}/gen-cpp/ComposePostService.cpp
    ${CMAKE_SOURCE_DIR}/gen-cpp/composepost_types.cpp
)

# Server executable
add_executable(composepost_server
    src/server.cpp
    ${GENERATED_SOURCES}
)

target_include_directories(composepost_server PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${THRIFT_INCLUDE_DIR}
)

target_link_libraries(composepost_server
    ${THRIFT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# Client executable
add_executable(composepost_client
    src/client.cpp
    ${GENERATED_SOURCES}
)

target_include_directories(composepost_client PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${THRIFT_INCLUDE_DIR}
)

target_link_libraries(composepost_client
    ${THRIFT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
