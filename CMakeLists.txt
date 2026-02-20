cmake_minimum_required(VERSION 3.28)

################################    MACROS    ################################

macro(project_add_inc)
  list(APPEND PROJECT_INCLUDES ${ARGV})
endmacro()

macro(project_add_src)
  list(APPEND PROJECT_SOURCES ${ARGV})
endmacro()

macro(project_add_inc_recursive)
  foreach (path ${ARGV})
    unset(found_inc)
    file(GLOB_RECURSE found_inc ${path}/*.h)
    list(APPEND PROJECT_INCLUDES ${found_inc})
  endforeach ()
endmacro()

macro(project_add_src_recursive)
  foreach (path ${ARGV})
    unset(found_src)
    file(GLOB_RECURSE found_src ${path}/*.cc)
    list(APPEND PROJECT_SOURCES ${found_src})
  endforeach ()
endmacro()

macro(feature name default)
  if (DEFINED ENV{${name}})
    set(${name} $ENV{${name}})
  else()
    set(${name} ${default})
  endif ()
  message(STATUS "Feature ${name} using value ${${name}}")
endmacro()


################################    PROJECT    ################################
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevent build in source directory")
endif ()


set(PROJECT_NAME       async)
set(PROJECT_DIR        ${CMAKE_CURRENT_LIST_DIR})
set(CMAKE_CXX_STANDARD 23)

feature(VALGRIND_DEBUG 0)

project(${PROJECT_NAME})


################################    SOURCES    ################################
project_add_inc_recursive(${PROJECT_DIR}/include)
project_add_src_recursive(${PROJECT_DIR}/src)

include_directories(
  ${PROJECT_DIR}/include
  ${PROJECT_DIR}/src
)

if (${VALGRIND_DEBUG} EQUAL 1)
  include_directories(/usr/local/include)
  add_definitions(-DVALGRIND_DEBUG=1)
endif ()


################################   EXECUTABLE   ################################
add_executable(${PROJECT_NAME}
    ${PROJECT_INCLUDES}
    ${PROJECT_SOURCES}
)
