project(judge-system)
cmake_minimum_required(VERSION 3.9.4)

if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
  message(FATAL_ERROR "Insufficient gcc version, need 8.0 or higher")
endif()

set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(MATRIX_JUDGE_TARGET judge-system)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

add_definitions(-DORMPP_ENABLE_MYSQL)

#  CMake control options
################################################################################
option(BUILD_UNIT_TEST "Build the unit test library" OFF)
option(BUILD_GTEST_MODULE_TEST "Build test for gtest module" OFF)

option(BUILD_ENTRY "Build the Judge System main entry" OFF)
################################################################################

# header directories
################################################################################
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ext/json/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ext/SimpleAmqpClient/src")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ext/cpp_redis/includes")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ext/cpp_redis/tacopie/includes")
################################################################################

# source files
################################################################################
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*main.cpp$")
file(GLOB ENTRY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
################################################################################

# Necessary libraries
################################################################################
find_package(Threads REQUIRED)
find_package(Boost 1.65 REQUIRED COMPONENTS program_options)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ext/glog")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ext/SimpleAmqpClient")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ext/cpp_redis")

if (BUILD_UNIT_TEST OR BUILD_GTEST_MODULE_TEST)
  if (NOT TARGET gtest)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ext/googletest")
  endif ()
endif ()
################################################################################

if (BUILD_UNIT_TEST)
  # Unit test header files
  ################################################################################
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ext/googletest/googletest/include")
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ext/googlemock/googlemock/include")
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/test/unit-test/")
  ################################################################################

  # Unit test source files
  ################################################################################
  file(GLOB_RECURSE TEST_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test/unit-test/*Test.cpp")
  file(GLOB TEST_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/test/unit-test/main.cpp")
  ################################################################################

  set(GTEST_TARGET "unit_test")
  add_executable(${GTEST_TARGET} ${TEST_SOURCE_FILES} ${TEST_MAIN} ${SOURCE_FILES})
  set_target_properties(${GTEST_TARGET}
    PROPERTIES
    CXX_STANDARD 14)
  target_link_libraries(${GTEST_TARGET}
    # TODO: add depended libraries
    glog
    gmock
    SimpleAmqpClient
    fmt
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    )
endif ()


if (BUILD_GTEST_MODULE_TEST)
  file(GLOB GTEST_TEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/test/main.cpp")
  set(GTEST_TEST_TARGET "gtest_test")
  add_executable(${GTEST_TEST_TARGET} ${GTEST_TEST_FILE})
  set_target_properties(${GTEST_TEST_TARGET}
    PROPERTIES
    CXX_STANDARD 17
    )
  target_link_libraries(${GTEST_TEST_TARGET}
    ${CMAKE_THREAD_LIBS_INIT}
    gmock
    )
endif ()

if (BUILD_ENTRY)
  add_executable(${MATRIX_JUDGE_TARGET} ${SOURCE_FILES} ${ENTRY_FILE})
  set_target_properties(${MATRIX_JUDGE_TARGET}
    PROPERTIES
    # ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
    # LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
    CXX_STANDARD 17
    )
  target_link_libraries(${MATRIX_JUDGE_TARGET}
    glog
    fmt
    SimpleAmqpClient
    mysqlclient
    curl
    cpp_redis
    boost_stacktrace_backtrace
    dl
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    )
  install(TARGETS ${MATRIX_JUDGE_TARGET} RUNTIME DESTINATION bin)
endif()
