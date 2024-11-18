# Include libfmt
add_subdirectory(external/fmt)
list(APPEND BELL_LIBS fmt::fmt)

# Include tao-json if not disabled
if(NOT BELL_DISABLE_TAOJSON)
    add_subdirectory(external/taojson)
    list(APPEND BELL_LIBS taocpp-json)
endif()