project(runguard)
cmake_minimum_required(VERSION 3.9.4)

if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
  message(FATAL_ERROR "Insufficient gcc version, need 7.0 or higher")
endif()

set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(BUILD_TARGET runguard)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# header directories
################################################################################
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../ext/fmt/include")
################################################################################

# source files
################################################################################
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*runguard.cpp$")
file(GLOB ENTRY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/runguard.cpp")
################################################################################

# Necessary libraries
################################################################################
find_package(Threads REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

# Force to build Release version
set(CMAKE_BUILD_TYPE_SAVED "${CMAKE_BUILD_TYPE}")
set(CMAKE_BUILD_TYPE Release)

    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../ext/glog" glog)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../ext/fmt" fmt)

set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE_SAVED}")
################################################################################

add_executable(${BUILD_TARGET} ${SOURCE_FILES} ${ENTRY_FILE})
set_target_properties(${BUILD_TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
    CXX_STANDARD 17
)
target_link_libraries(${BUILD_TARGET}
    glog
    cgroup
    fmt
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
install(TARGETS ${BUILD_TARGET} RUNTIME DESTINATION bin)
  