# Option for disabling message outputs from external dependencies
function(message)
    if(NOT "${ARGV}" STREQUAL "")
        # Check for the MESSAGE_QUIET flag
        if(NOT MESSAGE_QUIET)
            _message(${ARGV})
        endif()
    endif()
endfunction()

# Include libfmt
set(FMT_INSTALL OFF) # Disable fmt install targets
set(FMT_OS OFF) # Disable OS-specific features
add_subdirectory(external/fmt)
list(APPEND BELL_LIBS fmt::fmt)

# Include picohttpparser
add_subdirectory(external/picohttpparser)
list(APPEND BELL_LIBS picohttpparser)

# Include iqmath
add_subdirectory(external/iqmath)
list(APPEND BELL_LIBS iqmath)

# Include pthread
find_package(Threads REQUIRED)
list(APPEND BELL_LIBS Threads::Threads)

# Include span polyfill
add_subdirectory(external/span)
list(APPEND BELL_LIBS span)

# Include expected polyfill
add_subdirectory(external/expected)
list(APPEND BELL_LIBS expected-lite)

# Include tao-json if not disabled
if(NOT BELL_DISABLE_TAOJSON)
    add_subdirectory(external/taojson)
    list(APPEND BELL_LIBS taocpp-json)

    # # Suppress the deprecated literal operator error. TODO: report this upstream?
    # target_compile_options(taocpp-json INTERFACE -Wno-deprecated-literal-operator)
endif()

# Include MQTT if not disabled
if(NOT BELL_DISABLE_MQTT)
    add_subdirectory(external/mqtt)
    list(APPEND BELL_LIBS mqtt)
endif()

# Include miniz (gzip/deflate). Single-file amalgamation, public domain.
# Only HQLC currently needs it - nothing else in bell references mz_* at all.
if(NOT BELL_DISABLE_CODECS AND BELL_CODEC_HQLC)
    add_subdirectory(external/miniz)
    list(APPEND BELL_LIBS miniz)
endif()

if(NOT BELL_DISABLE_MBEDTLS)
    # Include mbedtls
    if(BELL_EXTERNAL_MBEDTLS)
        list(APPEND BELL_LIBS ${BELL_EXTERNAL_MBEDTLS})
    else()
        # Disable mbedtls tests and program targets
        set(ENABLE_TESTING OFF)
        set(ENABLE_PROGRAMS OFF)

        # add mbedtls as a subdirectory
        add_subdirectory(external/mbedtls)
        target_compile_options(mbedtls PRIVATE -O2)
        list(APPEND BELL_LIBS mbedtls)
    endif()
endif()

if(UNIX AND NOT APPLE)
    # Include avahi on linux
    list(APPEND BELL_LIBS avahi-client avahi-common)
endif()

if (NOT BELL_DISABLE_CONTAINERS AND BELL_CONTAINER_OGG)
    add_subdirectory(external/libogg)
    # Include ogg
    list(APPEND BELL_LIBS ogg)
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
    set(OPUS_BUILD_TESTING OFF)

    # Opus logs a lot of messages, so we disable them
    set(MESSAGE_QUIET ON)
    add_subdirectory(external/opus)
    add_subdirectory(external/opus-resample)
    set(MESSAGE_QUIET OFF)

    target_compile_options(opus PRIVATE -O2 -Wno-unused-parameter -Wno-parentheses-equality -Wno-cast-align -Wno-unused-but-set-variable -Wno-nonnull -Wno-stringop-overread)

    list(APPEND BELL_LIBS opus opus-resample)
endif()

# Audio codec - AAC-LC
if(NOT BELL_DISABLE_CODECS AND BELL_CODEC_AAC)
    add_subdirectory(external/fdk-aac)
    list(APPEND BELL_LIBS fdk-aac)
endif()

# Audio codec - MP3, Helix fixed-point decoder
if(NOT BELL_DISABLE_CODECS AND BELL_CODEC_MP3)
    add_subdirectory(external/libhelix-mp3)
    list(APPEND BELL_LIBS helix-mp3)
endif()

# Audio codec - Vorbis, tremor decoder
if(NOT BELL_DISABLE_CODECS AND BELL_CODEC_TREMOR)
    add_subdirectory(external/tremor)
    list(APPEND BELL_LIBS vorbisidec)
endif()

# Audio codec - LC3
if(NOT BELL_DISABLE_CODECS AND BELL_CODEC_LC3PLUS)
    add_subdirectory(external/liblc3)
    list(APPEND BELL_LIBS lc3)
endif()

# Audio codec - HQLC
if(NOT BELL_DISABLE_CODECS AND BELL_CODEC_HQLC)
    add_subdirectory(external/hqlc)
    list(APPEND BELL_LIBS hqlc)
endif()

# Audio codec - FLAC, libFLAC decoder
if(NOT BELL_DISABLE_CODECS AND BELL_CODEC_FLAC)
    # libFLAC build configuration - library only, decode-only usage
    set(BUILD_CXXLIBS OFF)
    set(BUILD_PROGRAMS OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_DOCS OFF)
    set(BUILD_SHARED_LIBS OFF)
    set(WITH_OGG OFF)
    set(ENABLE_64_BIT_WORDS OFF)
    set(ENABLE_MULTITHREADING OFF)
    set(INSTALL_MANPAGES OFF)
    set(INSTALL_PKGCONFIG_MODULES OFF)
    set(INSTALL_CMAKE_CONFIG_MODULE OFF)

    # libFLAC logs a lot of messages, so we disable them
    set(MESSAGE_QUIET ON)
    add_subdirectory(external/flac)
    set(MESSAGE_QUIET OFF)

    # Upstream libFLAC isn't clean against ESP-IDF's stricter project cflags
    target_compile_options(FLAC PRIVATE -Wno-error=inline -Wno-error=undef
                                        -Wno-error=format
                                        -Wno-error=incompatible-pointer-types)

    # src/CMakeLists.txt always adds these regardless of BUILD_PROGRAMS/
    # BUILD_CXXLIBS - unused here, keep them out of the default build set.
    foreach(unused_flac_target replaygain_analysis replaygain_synthesis
                               getopt grabbag utf8)
        set_target_properties(${unused_flac_target} PROPERTIES
                              EXCLUDE_FROM_ALL TRUE)
    endforeach()

    list(APPEND BELL_LIBS FLAC)
endif()

# Audio backends
if(BELL_BACKEND_PORTAUDIO)
    find_package(Portaudio REQUIRED)
    list(APPEND BELL_LIBS ${PORTAUDIO_LIBRARIES})
    list(APPEND BELL_EXTERNAL_INCLUDES ${PORTAUDIO_INCLUDE_DIRS})
endif()

# Espressif-specific dependencies
if(ESP_PLATFORM)
    list(APPEND BELL_LIBS idf::mbedtls idf::pthread idf::driver idf::lwip )

    if(IDF_VERSION_MAJOR LESS_EQUAL 4)
        if(NOT BELL_DISABLE_MDNS)
            list(APPEND BELL_LIBS idf::mdns)
        endif()
    else()
        if(NOT BELL_DISABLE_MDNS)
            list(APPEND BELL_LIBS idf::espressif__mdns)
        endif()
    endif()
endif()
