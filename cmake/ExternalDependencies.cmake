# Option for disabling message outputs from external dependencies
function(message)
    if(NOT MESSAGE_QUIET)
        _message(${ARGN})
    endif()
endfunction()

# Include libfmt
add_subdirectory(external/fmt)
list(APPEND BELL_LIBS fmt::fmt)

# Include picohttpparser
add_subdirectory(external/picohttpparser)
list(APPEND BELL_LIBS picohttpparser)

# Include pthread
find_package(Threads REQUIRED)
list(APPEND BELL_LIBS Threads::Threads)

# Include tao-json if not disabled
if(NOT BELL_DISABLE_TAOJSON)
    add_subdirectory(external/taojson)
    list(APPEND BELL_LIBS taocpp-json)
endif()

if (UNIX AND NOT APPLE)
    # Include avahi on linux
    list(APPEND BELL_LIBS avahi-client avahi-common)
endif()

# Audio codec - Opus and Opus resampler
if(NOT BELL_DISABLE_CODECS AND BELL_CODEC_OPUS)
    # Opus build configuration
    set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
    set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
    set(OPUS_MAY_HAVE_NEON OFF)
    set(OPUS_FIXED_POINT ON)
    set(OPUS_USE_ALLOCA ON)
    set(HAVE_LRINT ON)
    set(HAVE_LRINTF ON)

    # Opus logs a lot of messages, so we disable them
    set(MESSAGE_QUIET ON)
    add_subdirectory(external/opus)
    add_subdirectory(external/opus-resample)
    set(MESSAGE_QUIET OFF)

    target_compile_options(opus PRIVATE -O2 -Wno-unused-parameter -Wno-parentheses-equality -Wno-cast-align -Wno-unused-but-set-variable -Wno-nonnull)

    list(APPEND BELL_LIBS opus)
endif()
