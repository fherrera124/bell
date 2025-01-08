cmake_minimum_required(VERSION 3.16)

project(bell)

# CMake options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Custom build options
include(cmake/BuildOptions.cmake)

# External dependencies
include(cmake/ExternalDependencies.cmake)

# Own sources, tests
include(cmake/LibrarySources.cmake)

add_library(bell STATIC ${BELL_SOURCES})

target_link_libraries(bell PUBLIC ${BELL_LIBS})
target_include_directories(bell PUBLIC ${BELL_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

# Set flags
if(BELL_DISABLE_TAOJSON)
    target_compile_definitions(bell PUBLIC BELL_DISABLE_TAOJSON)
endif()
