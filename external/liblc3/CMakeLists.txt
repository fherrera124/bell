cmake_minimum_required(VERSION 3.10)

project(liblc3 C)

# Source files
set(LC3_SOURCES
    src/attdet.c
    src/bits.c
    src/bwdet.c
    src/energy.c
    src/lc3.c
    src/ltpf.c
    src/mdct.c
    src/plc.c
    src/sns.c
    src/spec.c
    src/tables.c
    src/tns.c
)

# Create static library
add_library(lc3 STATIC ${LC3_SOURCES})

# Include directories
target_include_directories(lc3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler flags for optimization
target_compile_options(lc3 PRIVATE
    -ffast-math
    -O3
)

# Enable LC3 Plus and High-Resolution mode support
target_compile_definitions(lc3 PUBLIC
    LC3_PLUS=1
    LC3_PLUS_HR=1
)

# ESP32-specific optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "xtensa")
    target_compile_options(lc3 PRIVATE
        -mlongcalls
    )
endif()

# Set C standard
set_target_properties(lc3 PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
