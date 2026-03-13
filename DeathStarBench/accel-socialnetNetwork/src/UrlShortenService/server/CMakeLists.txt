cmake_minimum_required(VERSION 3.10)
project(UrlShortenService)

# Debug build type
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG_LOGGING -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_TRACING")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_TRACING -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_CEREBELLUM -DENABLE_GEM5")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG_LOGGING")

find_package(Boost REQUIRED COMPONENTS log log_setup)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(THRIFT REQUIRED thrift)
pkg_check_modules(LIBMEMCACHED REQUIRED libmemcached)
pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)

add_executable(
    UrlShortenService
    main_server.cpp
    UrlShortenHandler.cpp
    UrlShortenBusinessLogic.cpp
    ${THRIFT_GEN_CPP_DIR}/UrlShortenService.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_types.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_constants.cpp
)

target_include_directories(
    UrlShortenService PRIVATE
    ${LIBMEMCACHED_INCLUDE_DIR}
    /usr/include/libmongoc-1.0
    /usr/include/libbson-1.0
    .
)

target_link_directories(UrlShortenService PRIVATE /usr/local/lib)

target_link_libraries(
    UrlShortenService
    ${LIBMEMCACHED_LIBRARIES}
    ${MONGOC_LIBRARIES}
    thrift
    pthread
    Boost::log
    Boost::log_setup
)

install(TARGETS UrlShortenService DESTINATION ./)
