cmake_minimum_required(VERSION 3.10)
project(UrlShortenService)

# Debug build type
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

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
    ${MONGOC_INCLUDE_DIRS}
    ${LIBMEMCACHED_INCLUDE_DIRS}
    .
)

target_link_directories(UrlShortenService PRIVATE /usr/local/lib)

target_link_libraries(
    UrlShortenService
    nlohmann_json
    ${THRIFT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    Boost::log
    Boost::log_setup
    ${MONGOC_LIBRARIES}
    ${LIBMEMCACHED_LIBRARIES}
)
