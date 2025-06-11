cmake_minimum_required(VERSION 3.10)
project(minimal_social)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Thrift custom installation paths
set(THRIFT_ROOT "/usr/local" CACHE PATH "Thrift installation directory")
set(THRIFT_INCLUDE_DIR "${THRIFT_ROOT}/include")
set(THRIFT_LIB_DIR "${THRIFT_ROOT}/lib")

# Find Thrift
find_path(THRIFT_INCLUDE_DIRS thrift/Thrift.h
    PATHS ${THRIFT_INCLUDE_DIR} /usr/include /usr/local/include
)
find_library(THRIFT_LIBRARIES thrift
    PATHS ${THRIFT_LIB_DIR} /usr/lib /usr/local/lib
)

# Add includes
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/gen-cpp
    ${THRIFT_INCLUDE_DIRS}
)

# Source files for the library
set(MINIMAL_SOCIAL_SOURCES
    ${CMAKE_SOURCE_DIR}/gen-cpp/ComposePostService.cpp
    ${CMAKE_SOURCE_DIR}/gen-cpp/UserService.cpp
    ${CMAKE_SOURCE_DIR}/gen-cpp/UniqueIdService.cpp
    ${CMAKE_SOURCE_DIR}/gen-cpp/minimal_social_types.cpp
)

# Add library
add_library(minimal_social_lib ${MINIMAL_SOCIAL_SOURCES})

# Add executables
add_executable(compose_post_server ComposePostServer.cpp)
add_executable(user_server UserServer.cpp)
add_executable(unique_id_server UniqueIdServer.cpp)
add_executable(test_client TestClient.cpp)

# Link libraries
target_link_libraries(minimal_social_lib
    ${THRIFT_LIBRARIES}
)

target_link_libraries(compose_post_server
    minimal_social_lib
    pthread
)

target_link_libraries(user_server
    minimal_social_lib
    pthread
)

target_link_libraries(unique_id_server
    minimal_social_lib
    pthread
)

target_link_libraries(test_client
    minimal_social_lib
)

# Set RPATH for custom library locations
set(CMAKE_INSTALL_RPATH ${THRIFT_LIB_DIR})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
