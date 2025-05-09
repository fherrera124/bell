# # Option for disabling message outputs from external dependencies
# function(message)
#     if(NOT MESSAGE_QUIET)
#         _message(${ARGN})
#     endif()
# endfunction()

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

# Include tao-json if not disabled
if(NOT BELL_DISABLE_TAOJSON)
    add_subdirectory(external/taojson)
    list(APPEND BELL_LIBS taocpp-json)
endif()

# Include MQTT if not disabled
if(NOT BELL_DISABLE_MQTT)
    add_subdirectory(external/mqtt)
    list(APPEND BELL_LIBS mqtt)
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
        list(APPEND BELL_LIBS mbedtls)
    endif()
endif()

if(UNIX AND NOT APPLE)
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

    target_compile_options(opus PRIVATE -O2 -Wno-unused-parameter -Wno-parentheses-equality -Wno-cast-align -Wno-unused-but-set-variable -Wno-nonnull -Wno-stringop-overread)

    list(APPEND BELL_LIBS opus)
endif()

# Audio backends
if(BELL_BACKEND_PORTAUDIO)
    find_package(Portaudio REQUIRED)
    list(APPEND BELL_LIBS ${PORTAUDIO_LIBRARIES})
    list(APPEND BELL_EXTERNAL_INCLUDES ${PORTAUDIO_INCLUDE_DIRS})
endif()

# Espressif-specific dependencies
if(ESP_PLATFORM)
    list(APPEND BELL_LIBS idf::mbedtls idf::pthread idf::driver idf::lwip idf::newlib)

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
