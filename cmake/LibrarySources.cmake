# Enable testing
if(NOT BELL_DISABLE_TESTS)
    add_subdirectory(external/catch2)
    enable_testing()
    add_subdirectory(test)
endif()

# All includes are referenced from the root directory as "bell/"
list(APPEND BELL_INCLUDES "include")

# Main library sources
file(GLOB BELL_SOURCES
    "main/io/*.cpp" # bell::io
    "main/net/*.cpp" # bell::net
    "main/http/*.cpp" # bell::http
    "main/utils/*.cpp" # bell::utils
    "main/audio/*.cpp" # bell::audio
    "main/dsp/*.cpp" # bell::dsp
)

# Add platform-specific sources
if(APPLE)
    file(GLOB BELL_SOURCES_APPLE "main/platform/apple/*.cpp")
    list(APPEND BELL_SOURCES ${BELL_SOURCES_APPLE})
endif()

# Unix common includes
if(UNIX)
    file(GLOB BELL_SOURCES_POSIX "main/platform/posix/*.cpp")
    list(APPEND BELL_SOURCES ${BELL_SOURCES_POSIX})
endif()

# Linux includes
if (UNIX AND NOT APPLE)
    file(GLOB BELL_SOURCES_POSIX "main/platform/linux/*.cpp")
    list(APPEND BELL_SOURCES ${BELL_SOURCES_POSIX})
endif()