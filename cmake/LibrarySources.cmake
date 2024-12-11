# Enable testing
if(NOT BELL_DISABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

set(BELL_IO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/io")
set(BELL_UTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/utils")

# All includes are referenced from the root directory as "bell/"
list(APPEND BELL_INCLUDES "include")

# Main library sources
file(GLOB BELL_SOURCES
    "main/io/*.cpp" # bell::io
    "main/utils/*.cpp" # bell::utils
    "main/audio/*.cpp" # bell::audio
    "main/dsp/*.cpp" # bell::dsp
)