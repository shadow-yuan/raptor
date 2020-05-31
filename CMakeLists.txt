cmake_minimum_required(VERSION 3.10)
project(raptor VERSION 1.0.0 LANGUAGES C CXX)

# This project can use C11, but will gracefully decay down to C89.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS OFF)

# This project requires C++11.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RAPTOR_VERSION_MAJOR "1")
set(RAPTOR_VERSION_MINOR "0")
set(RAPTOR_VERSION_PATCH "0")
set(RAPTOR_VERSION
    "${RAPTOR_VERSION_MAJOR}.${RAPTOR_VERSION_MINOR}.${RAPTOR_VERSION_PATCH}")

string(REPLACE "." ","  RAPTOR_RC_FILEVERSION "${RAPTOR_VERSION}")
message(STATUS "Raptor Version     : ${RAPTOR_VERSION}")

if(WIN32 AND MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/MT")
    set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
    set(RAPTOR_BASELIB_LIBRARIES wsock32 ws2_32)
    set(RAPTOR_VERSIONINFO_RC "${CMAKE_BINARY_DIR}/version.rc")
    configure_file("${CMAKE_SOURCE_DIR}/version.rc.in"
                   "${RAPTOR_VERSIONINFO_RC}")
else()
    set(RAPTOR_BASELIB_LIBRARIES m pthread)
    set(RAPTOR_VERSIONINFO_RC "")
endif()

# Build option
option(RAPTOR_BUILD_ALLTESTS     "Build raptor's all unit tests" OFF)

if(WIN32 AND MSVC)
    add_definitions(/W4)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_definitions(-Wall)
    add_compile_options(-fvisibility=hidden)
endif()

include_directories(
    "${PROJECT_BINARY_DIR}/include"
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
)

# static library
add_library(raptor-static STATIC "")

# shared library (dll or so)
add_library(raptor-shared SHARED "" ${RAPTOR_VERSIONINFO_RC})

if (WIN32)
    set(
        RAPTOR_ENGINE_SOURCE
        #IOCP
        "${PROJECT_SOURCE_DIR}/core/windows/connection.cc"
        "${PROJECT_SOURCE_DIR}/core/windows/iocp_thread.cc"
        "${PROJECT_SOURCE_DIR}/core/windows/iocp.cc"
        "${PROJECT_SOURCE_DIR}/core/windows/socket_setting.cc"
        "${PROJECT_SOURCE_DIR}/core/windows/tcp_client.cc"
        "${PROJECT_SOURCE_DIR}/core/windows/tcp_listener.cc"
        "${PROJECT_SOURCE_DIR}/core/windows/tcp_server.cc"
    )
else()
    set(
        RAPTOR_ENGINE_SOURCE
        #EPOLL
        "${PROJECT_SOURCE_DIR}/core/linux/connection.cc"
        "${PROJECT_SOURCE_DIR}/core/linux/epoll_thread.cc"
        "${PROJECT_SOURCE_DIR}/core/linux/epoll.cc"
        "${PROJECT_SOURCE_DIR}/core/linux/socket_setting.cc"
        "${PROJECT_SOURCE_DIR}/core/linux/tcp_client.cc"
        "${PROJECT_SOURCE_DIR}/core/linux/tcp_listener.cc"
        "${PROJECT_SOURCE_DIR}/core/linux/tcp_server.cc"
    )
endif()

set(
    RAPTOR_CORE_SOURCE
    "${PROJECT_SOURCE_DIR}/core/slice/slice_buffer.cc"
    "${PROJECT_SOURCE_DIR}/core/slice/slice.cc"
    "${PROJECT_SOURCE_DIR}/core/host_port.cc"
    "${PROJECT_SOURCE_DIR}/core/mpscq.cc"
    "${PROJECT_SOURCE_DIR}/core/resolve_address.cc"
    "${PROJECT_SOURCE_DIR}/core/socket_util.cc"
)
set(
    RAPTOR_UTIL_SOURCE
    "${PROJECT_SOURCE_DIR}/util/alloc.cc"
    "${PROJECT_SOURCE_DIR}/util/cpu.cc"
    "${PROJECT_SOURCE_DIR}/util/list_entry.cc"
    "${PROJECT_SOURCE_DIR}/util/log.cc"
    "${PROJECT_SOURCE_DIR}/util/status.cc"
    "${PROJECT_SOURCE_DIR}/util/string.cc"
    "${PROJECT_SOURCE_DIR}/util/sync.cc"
    "${PROJECT_SOURCE_DIR}/util/thread.cc"
    "${PROJECT_SOURCE_DIR}/util/time.cc"
)

set(
    RAPTOR_SURFACE_SOURCE
    "${PROJECT_SOURCE_DIR}/surface/adapter.cc"
    "${PROJECT_SOURCE_DIR}/surface/c.cc"
    "${PROJECT_SOURCE_DIR}/surface/client.cc"
    "${PROJECT_SOURCE_DIR}/surface/server.cc"
)

target_sources(raptor-static
    PRIVATE
    ${RAPTOR_ENGINE_SOURCE}
    ${RAPTOR_CORE_SOURCE}
    ${RAPTOR_UTIL_SOURCE}
    ${RAPTOR_SURFACE_SOURCE}
)

target_sources(raptor-shared
    PRIVATE
    ${RAPTOR_ENGINE_SOURCE}
    ${RAPTOR_CORE_SOURCE}
    ${RAPTOR_UTIL_SOURCE}
    ${RAPTOR_SURFACE_SOURCE}
)

target_compile_definitions(raptor-shared
    PUBLIC
    # Used by raptor/export.h when building shared library.
    RAPTOR_SHARED_LIBRARY
    RAPTOR_COMPILE_LIBRARY
)

target_link_libraries(raptor-static
    ${RAPTOR_BASELIB_LIBRARIES}
)
target_link_libraries(raptor-shared
    ${RAPTOR_BASELIB_LIBRARIES}
)

if(RAPTOR_BUILD_ALLTESTS)
    enable_testing()

    function(raptor_test test_file)
    get_filename_component(test_target_name "${test_file}" NAME_WE)

    add_executable("${test_target_name}" "")
    target_sources("${test_target_name}"
        PRIVATE
        "${PROJECT_SOURCE_DIR}/util/test_util.cc"
        "${PROJECT_SOURCE_DIR}/util/test_util.h"

        "${test_file}"
    )
    target_link_libraries("${test_target_name}" raptor-static)
    add_test(NAME "${test_target_name}" COMMAND "${test_target_name}")
    endfunction(raptor_test)

    # Add unit test source files below
    #raptor_test("${PROJECT_SOURCE_DIR}/tests/slice_test.cc")

endif(RAPTOR_BUILD_ALLTESTS)

if (WIN32)
    set_target_properties(raptor-static PROPERTIES OUTPUT_NAME libraptor CLEAN_DIRECT_OUTPUT 1)
else()
    set_target_properties(raptor-static PROPERTIES OUTPUT_NAME raptor CLEAN_DIRECT_OUTPUT 1)
endif()
set_target_properties(raptor-shared PROPERTIES OUTPUT_NAME raptor CLEAN_DIRECT_OUTPUT 1)
