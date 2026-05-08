cmake_minimum_required(VERSION 3.16)

project(dlms_xdlms
  VERSION 0.1.0
  LANGUAGES CXX)

option(DLMS_BUILD_TESTS "Build GoogleTest tests" ON)
option(DLMS_USE_SYSTEM_GTEST "Use system-installed GoogleTest" OFF)

add_library(dlms_xdlms
  src/xdlms/xdlms_client.cpp
  src/xdlms/xdlms_server.cpp
  src/xdlms/xdlms_status.cpp
  src/xdlms/xdlms_types.cpp)

if(NOT TARGET dlms_association AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dlms-association/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../dlms-association" "${CMAKE_CURRENT_BINARY_DIR}/_deps/dlms-association")
endif()

if(NOT TARGET dlms_profile AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dlms-profile/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../dlms-profile" "${CMAKE_CURRENT_BINARY_DIR}/_deps/dlms-profile")
endif()

if(NOT TARGET dlms_apdu AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dlms-apdu/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../dlms-apdu" "${CMAKE_CURRENT_BINARY_DIR}/_deps/dlms-apdu")
endif()

if(NOT TARGET dlms_security AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../dlms-security/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../dlms-security" "${CMAKE_CURRENT_BINARY_DIR}/_deps/dlms-security")
endif()

target_compile_features(dlms_xdlms PUBLIC cxx_std_11)

target_include_directories(dlms_xdlms
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(dlms_xdlms
  PUBLIC
    dlms_association
    dlms_profile
    dlms_apdu
    dlms_security)

set(DLMS_XDLMS_HAS_TYPES_API ON CACHE INTERNAL
  "dlms-xdlms exposes xDLMS service types")

if(DLMS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
