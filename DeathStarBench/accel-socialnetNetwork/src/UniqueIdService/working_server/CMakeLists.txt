cmake_minimum_required(VERSION 3.10)
project(UniqueIdService)

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
