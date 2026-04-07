cmake_minimum_required(VERSION 3.10)
project(PostStorageService)

# Debug build type
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DENABLE_GEM5")
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
    PostStorageService
    main_server.cpp
    PostStorageHandler.cpp
    PostStorageBusinessLogic.cpp
    ${THRIFT_GEN_CPP_DIR}/PostStorageService.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_types.cpp
    ${THRIFT_GEN_CPP_DIR}/social_network_constants.cpp
)

#add_compile_definitions(DEBUG_LOGGING)
#add_compile_definitions(ENABLE_POSTSTORAGE_GEM5_SERVER)
target_compile_definitions(
    ${BINARY_NAME} PRIVATE
    ENABLE_POSTSTORAGE_GEM5_SERVER
)

target_include_directories(
    PostStorageService PRIVATE
    ${LIBMEMCACHED_INCLUDE_DIR}
    #${MONGOC_INCLUDE_DIRS}
    /usr/include/libmongoc-1.0
    /usr/include/libbson-1.0
    .
)

target_link_directories(PostStorageService PRIVATE /usr/local/lib)

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
