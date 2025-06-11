cmake_minimum_required(VERSION 3.10)
project(ComposePost)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add debug information
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# Find required packages
find_package(Threads REQUIRED)

# Set Thrift paths manually since it was installed from source
set(THRIFT_INCLUDE_DIR "/usr/local/include")
set(THRIFT_LIBRARY_DIR "/usr/local/lib")
set(THRIFT_LIBRARIES "${THRIFT_LIBRARY_DIR}/libthrift.so")

# Add include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${THRIFT_INCLUDE_DIR}
)

# Add library directories
link_directories(
    ${THRIFT_LIBRARY_DIR}
)

# Generate Thrift files
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/gen-cpp/ComposePostService.cpp
        ${CMAKE_SOURCE_DIR}/gen-cpp/ComposePostService.h
        ${CMAKE_SOURCE_DIR}/gen-cpp/composepost_types.cpp
        ${CMAKE_SOURCE_DIR}/gen-cpp/composepost_types.h
    COMMAND thrift -r --gen cpp ${CMAKE_SOURCE_DIR}/composepost.thrift
    DEPENDS ${CMAKE_SOURCE_DIR}/composepost.thrift
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Create a library for the generated thrift files
add_library(composepost_thrift
    gen-cpp/ComposePostService.cpp
    gen-cpp/composepost_types.cpp
)

target_link_libraries(composepost_thrift
    ${THRIFT_LIBRARIES}
)

# Server executable
add_executable(composepost_server
    src/server.cpp
)

target_link_libraries(composepost_server
    composepost_thrift
    ${THRIFT_LIBRARIES}
    Threads::Threads
)

# Client executable
add_executable(composepost_client
    src/client.cpp
)

target_link_libraries(composepost_client
    composepost_thrift
    ${THRIFT_LIBRARIES}
    Threads::Threads
)

# Add rpath so executables can find shared libraries
set_target_properties(composepost_server composepost_client
    PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "/usr/local/lib"
)

# Install targets
install(TARGETS composepost_server composepost_client
    RUNTIME DESTINATION bin
)
