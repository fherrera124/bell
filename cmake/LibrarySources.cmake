# Enable testing
if(NOT BELL_DISABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

set(BELL_IO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/io")
set(BELL_UTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/utils")

list(APPEND BELL_INCLUDES "main/utils/include" "main/io/include")

# Main library sources
file(GLOB BELL_SOURCES
    "main/io/*.cpp" # bell::io
    "main/utils/*.cpp" # bell::utils
)