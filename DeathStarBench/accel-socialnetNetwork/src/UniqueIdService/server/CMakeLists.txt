cmake_minimum_required(VERSION 3.10)
project(UniqueIdService)

# Debug build type
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_TRACING -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O3 -DDEBUG_LOGGING")

find_package(Boost REQUIRED COMPONENTS log log_setup)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(THRIFT REQUIRED thrift)

add_executable(
    UniqueIdService
    main_server.cpp
    UniqueIdHandler.cpp
    UniqueIdBusinessLogic.cpp
    ${THRIFT_GEN_CPP_DIR}/UniqueIdService.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_types.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_constants.cpp
)

#add_compile_definitions(DEBUG_LOGGING)

target_include_directories(
    UniqueIdService PRIVATE
    #/usr/local/include/jaegertracing
    .
)

target_link_libraries(
    UniqueIdService
    nlohmann_json
    ${THRIFT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    Boost::log
    Boost::log_setup
    #jaegertracing
)
