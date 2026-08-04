# Enable testing
if(NOT BELL_DISABLE_TESTS)
    # Trompeloeil for mocking
    add_subdirectory(external/trompeloeil)
    # Doctest for unit testing
    add_subdirectory(external/doctest)
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
    "main/dsp/*.cpp" # bell::dsp
)

# main/audio/*.cpp is intentionally NOT globbed as a whole: every file in
# there #includes its one third-party codec/container/backend library's
# header directly (aacdecoder_lib.h, opus.h, ivorbiscodec.h, lc3_cpp.h,
# hqlc's own header, ogg/ogg.h, portaudio.h) - compiling any of them while
# that dependency is disabled (and so never add_subdirectory'd in
# ExternalDependencies.cmake) fails with a missing-header error, not a
# silently-skipped file.
if(NOT BELL_DISABLE_CODECS)
    if(BELL_CODEC_AAC)
        list(APPEND BELL_SOURCES "main/audio/AACCodec.cpp")
    endif()
    if(BELL_CODEC_OPUS)
        list(APPEND BELL_SOURCES "main/audio/OpusCodec.cpp")
    endif()
    if(BELL_CODEC_TREMOR)
        list(APPEND BELL_SOURCES "main/audio/TremorVorbisCodec.cpp")
    endif()
    if(BELL_CODEC_LC3PLUS)
        list(APPEND BELL_SOURCES "main/audio/LC3plusCodec.cpp")
    endif()
    if(BELL_CODEC_HQLC)
        list(APPEND BELL_SOURCES "main/audio/HQLCCodec.cpp")
    endif()
endif()
if(NOT BELL_DISABLE_CONTAINERS AND BELL_CONTAINER_OGG)
    list(APPEND BELL_SOURCES "main/audio/OggContainer.cpp")
endif()
if(BELL_BACKEND_PORTAUDIO)
    list(APPEND BELL_SOURCES "main/audio/PortAudioBackend.cpp")
endif()

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

# Espressif includes
if (ESP_PLATFORM)
    file(GLOB BELL_SOURCES_ESP "main/platform/esp/*.cpp" "main/platform/esp/*.S")
    list(APPEND BELL_SOURCES ${BELL_SOURCES_ESP})
endif()
