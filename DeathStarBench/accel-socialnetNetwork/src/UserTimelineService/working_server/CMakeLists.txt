cmake_minimum_required(VERSION 3.10)
project(UserTimelineService)

# Debug build type
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG_LOGGING -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_TRACING")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_TRACING -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_CEREBELLUM -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG_LOGGING")

find_package(Boost REQUIRED COMPONENTS log log_setup program_options)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(THRIFT REQUIRED thrift)
pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)

add_executable(
    UserTimelineService
    main_server.cpp
    UserTimelineHandler.cpp
    UserTimelineBusinessLogic.cpp
    ${THRIFT_GEN_CPP_DIR}/UserTimelineService.cpp
    ${THRIFT_GEN_CPP_DIR}/PostStorageService.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_types.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_constants.cpp
)

#add_compile_definitions(DEBUG_LOGGING)

target_include_directories(
    UserTimelineService PRIVATE
    /usr/include/libmongoc-1.0
    /usr/include/libbson-1.0
    /usr/local/include/jaegertracing
    /usr/local/include/hiredis
    /usr/local/include/sw
    .
)

target_link_libraries(
    UserTimelineService
    ${MONGOC_LIBRARIES}
    nlohmann_json::nlohmann_json
    thrift
    pthread
    Boost::log
    Boost::log_setup
    Boost::program_options
    #jaegertracing
    #/usr/lib/x86_64-linux-gnu/libhiredis.a
    /usr/local/lib/libhiredis.a
    /usr/local/lib/libhiredis_ssl.a
    /usr/local/lib/libredis++.a
    #hiredis
    #redis++
    OpenSSL::SSL
)

install(TARGETS UserTimelineService DESTINATION ./)
