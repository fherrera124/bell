cmake_minimum_required(VERSION 3.16)

project(bell)

# CMake options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Custom build options
include(cmake/BuildOptions.cmake)

# External dependencies
include(cmake/ExternalDependencies.cmake)

# Own sources, tests
include(cmake/LibrarySources.cmake)

add_library(bell STATIC ${BELL_SOURCES})

target_link_libraries(bell PUBLIC ${BELL_LIBS})
target_include_directories(bell PUBLIC ${BELL_INCLUDES} ${BELL_EXTERNAL_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

# Force bell's vendored fmt include to win over anything system-provided
# (e.g. /opt/homebrew/include when consumers pull PortAudio via pkg-config).
# Homebrew ships a newer fmt that removed fmt::localtime, which breaks
# Logger.cpp if its header is picked up instead of bell's pinned fmt.
target_include_directories(bell BEFORE PUBLIC
    $<TARGET_PROPERTY:fmt::fmt,INTERFACE_INCLUDE_DIRECTORIES>
)

# Set flags
if(BELL_DISABLE_TAOJSON)
    target_compile_definitions(bell PUBLIC BELL_DISABLE_TAOJSON)
endif()

if(BELL_DISABLE_MDNS)
    target_compile_definitions(bell PUBLIC BELL_DISABLE_MDNS)
endif()

if(BELL_DISABLE_MBEDTLS)
    target_compile_definitions(bell PUBLIC BELL_DISABLE_MBEDTLS)
endif()

if(BELL_BACKEND_PORTAUDIO)
    target_compile_definitions(bell PUBLIC BELL_BACKEND_PORTAUDIO)
endif()

if(NOT BELL_DISABLE_SANITIZERS)
    # Enable sanitizers
    target_compile_options(bell PUBLIC -fsanitize=address -fsanitize=undefined)
    target_link_options(bell PUBLIC -fsanitize=address -fsanitize=undefined)
endif()
