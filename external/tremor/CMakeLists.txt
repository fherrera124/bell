cmake_minimum_required(VERSION 3.16)

set(TREMOR_SRCS
  "bitwise.c"
  "floor0.c"
  "framing.c"
  "mapping0.c"
  "res012.c"
  "codebook.c"
  "floor1.c"
  "info.c"
  "mdct.c"
  "vorbisfile.c"
  "dsp.c"
  "floor_lookup.c"
  "misc.c"
)

project(tremor)

# Add the library target
add_library(tremor STATIC ${TREMOR_SRCS})

target_include_directories(tremor PUBLIC ".")
