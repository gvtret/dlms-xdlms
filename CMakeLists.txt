cmake_minimum_required(VERSION 3.16)

project(dlms_xdlms_client
  VERSION 0.1.0
  LANGUAGES CXX)

option(DLMS_BUILD_TESTS "Build GoogleTest tests" ON)
option(DLMS_USE_SYSTEM_GTEST "Use system-installed GoogleTest" OFF)

add_library(dlms_xdlms_client
  src/xdlms_client/xdlms_client_status.cpp
  src/xdlms_client/xdlms_client_types.cpp)

target_compile_features(dlms_xdlms_client PUBLIC cxx_std_11)

target_include_directories(dlms_xdlms_client
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(DLMS_XDLMS_CLIENT_HAS_TYPES_API ON CACHE INTERNAL
  "dlms-xdlms-client exposes xDLMS client service types")

if(DLMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
