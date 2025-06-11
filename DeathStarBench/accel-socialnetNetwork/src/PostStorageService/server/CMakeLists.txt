cmake_minimum_required(VERSION 3.10)
project(PostStorageService)

find_package(Boost REQUIRED COMPONENTS log log_setup)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(THRIFT REQUIRED thrift)
pkg_check_modules(LIBMEMCACHED REQUIRED libmemcached)
pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)

add_executable(
    PostStorageService
    main_server.cpp
    PostStorageHandler.cpp
    PostStorageBusinessLogic.cpp
    ${THRIFT_GEN_CPP_DIR}/PostStorageService.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_types.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_constants.cpp
)

#add_compile_definitions(DEBUG_LOGGING)

target_include_directories(
    PostStorageService PRIVATE
    ${LIBMEMCACHED_INCLUDE_DIR}
    #${MONGOC_INCLUDE_DIRS}
    /usr/include/libmongoc-1.0
    /usr/include/libbson-1.0
    .
)

target_link_libraries(
    PostStorageService
    ${LIBMEMCACHED_LIBRARIES}
    ${MONGOC_LIBRARIES}
    thrift
    pthread
    Boost::log
    Boost::log_setup
)

install(TARGETS PostStorageService DESTINATION ./)
