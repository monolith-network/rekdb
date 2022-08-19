cmake_minimum_required(VERSION 3.8)

project(rekdb)

set(CMAKE_CXX_STANDARD 20)

#
# Options
#
option(COMPILE_TESTS "Execute unit tests" ON)
option(WITH_ASAN     "Compile with ASAN" OFF)

#
# Find Threads
#
find_package (Threads REQUIRED)

#
# Setup build type 'Release vs Debug'
#
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

#
# Locate CPPUTest
#
if(DEFINED ENV{CPPUTEST_HOME})
    message(STATUS "Using CppUTest home: $ENV{CPPUTEST_HOME}")
    set(CPPUTEST_INCLUDE_DIRS $ENV{CPPUTEST_HOME}/include)
    set(CPPUTEST_LIBRARIES $ENV{CPPUTEST_HOME}/lib)
    set(CPPUTEST_LDFLAGS CppUTest CppUTestExt)
else()
    find_package(PkgConfig REQUIRED)
    pkg_search_module(CPPUTEST REQUIRED cpputest>=3.8)
    message(STATUS "Found CppUTest version ${CPPUTEST_VERSION}")
endif()

#
# Setup ASAN
#
if(WITH_ASAN)
  set(CMAKE_CX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

#
#  Ensure that tld includes available everywhere
#
include_directories(
   ${CMAKE_SOURCE_DIR})

#
#  Add subdirectories
#
add_subdirectory(externals)

#
#  Sources
#
set(APP_SOURCES
   ${CMAKE_SOURCE_DIR}/server.cpp
)

#
#  Test
#
if(COMPILE_TESTS)
   add_subdirectory(tests)
endif()

#
#  Create the app
#
add_executable(rekdb
         ${APP_SOURCES}
         ${EXTERNALS_SOURCES}
         main.cpp)

#
#  Link 
#
target_link_libraries(rekdb
        PRIVATE
        ${NETTLE_LIBRARIES}
        Threads::Threads
        rocksdb dl
        )

#
#  Copy configs
#  
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/configs
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
