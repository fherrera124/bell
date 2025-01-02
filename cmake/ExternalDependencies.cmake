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