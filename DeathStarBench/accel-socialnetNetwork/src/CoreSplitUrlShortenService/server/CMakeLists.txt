cmake_minimum_required(VERSION 3.10)
project(UrlShortenService)

# Debug build type
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -no-pie -fno-pie")

# Uncomment to enable tracing
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_TRACING")

# Uncomment to enable Cerebellum
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_CEREBELLUM")
# set(ENABLE_CEREBELLUM ON)

# Uncomment to enable GEM5
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_GEM5")
# set(ENABLE_GEM5 ON)

# Set paths for GEM5
set(M5OPS_HEADER_PATH "/home/ubuntu/gem5/include")
set(M5_BUILD_PATH "/home/ubuntu/gem5/util/m5/build/x86")

find_package(Boost REQUIRED COMPONENTS log log_setup)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(THRIFT REQUIRED thrift)

set(BINARY_NAME "UrlShortenService.hw.local")
set(EXTRA_LIBS "")
set(EXTRA_INCLUDE_DIRS "")

if(ENABLE_CEREBELLUM)
    set(BINARY_NAME "${BINARY_NAME}.crb")
    list(APPEND EXTRA_LIBS "cerebellum")
endif()

if(ENABLE_GEM5)
    set(BINARY_NAME "${BINARY_NAME}.m5")
    list(APPEND EXTRA_LIBS "m5")
    if(M5OPS_HEADER_PATH)
        list(APPEND EXTRA_INCLUDE_DIRS "${M5OPS_HEADER_PATH}" "${M5OPS_HEADER_PATH}/../util/m5/src")
    endif()
    if(M5_BUILD_PATH)
        link_directories("${M5_BUILD_PATH}/out/")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie -Wl,-no-pie")
endif()

add_executable(
    ${BINARY_NAME}
    main_server.cpp
    UrlShortenHandler.cpp
    UrlShortenBusinessLogic.cpp
    ${THRIFT_GEN_CPP_DIR}/UrlShortenService.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_types.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_constants.cpp
)

target_compile_definitions(
    ${BINARY_NAME} PRIVATE
    ENABLE_CORE_SPLIT_URLSHORTEN_HANDLER
)

target_include_directories(
    ${BINARY_NAME} PRIVATE
    .
    ${EXTRA_INCLUDE_DIRS}
)

target_link_libraries(
    ${BINARY_NAME}
    nlohmann_json
    ${THRIFT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    Boost::log
    Boost::log_setup
    ${EXTRA_LIBS}
)

install(TARGETS ${BINARY_NAME} DESTINATION ./)
