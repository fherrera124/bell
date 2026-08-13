cmake_minimum_required(VERSION 3.10)

project(libhelix-mp3 C)

# Portable C subset of the upstream tree - testwrap/, docs/, and real/arm/ excluded.
set(HELIX_MP3_SOURCES
    mp3dec.c
    mp3tabs.c
    real/bitstream.c
    real/buffers.c
    real/dct32.c
    real/dequant.c
    real/dqchan.c
    real/huffman.c
    real/hufftabs.c
    real/imdct.c
    real/polyphase.c
    real/scalfact.c
    real/stproc.c
    real/subband.c
    real/trigtabs.c
)

add_library(helix-mp3 STATIC ${HELIX_MP3_SOURCES})

target_include_directories(helix-mp3 PRIVATE real)
target_include_directories(helix-mp3 PUBLIC pub)
