cmake_minimum_required(VERSION 3.16)

project(dlms_xdlms
  VERSION 0.1.0
  LANGUAGES CXX)

option(DLMS_BUILD_TESTS "Build GoogleTest tests" ON)
option(DLMS_USE_SYSTEM_GTEST "Use system-installed GoogleTest" OFF)

add_library(dlms_xdlms
  src/xdlms/xdlms_status.cpp
  src/xdlms/xdlms_types.cpp)

target_compile_features(dlms_xdlms PUBLIC cxx_std_11)

target_include_directories(dlms_xdlms
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(DLMS_XDLMS_HAS_TYPES_API ON CACHE INTERNAL
  "dlms-xdlms exposes xDLMS service types")

if(DLMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
