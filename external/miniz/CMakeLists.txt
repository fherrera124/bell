cmake_minimum_required(VERSION 3.16)
project(miniz)

# Header lives in bell_miniz/ so the include path is `bell_miniz/miniz.h` (esp-idf has miniz.h in rom that fights it)
add_library(miniz STATIC bell_miniz/miniz.c)
target_include_directories(miniz PUBLIC ".")

# Disable archive apis
target_compile_definitions(miniz PUBLIC
    MINIZ_NO_ARCHIVE_APIS
    MINIZ_NO_STDIO
    MINIZ_NO_TIME
)
