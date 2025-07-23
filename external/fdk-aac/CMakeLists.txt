cmake_minimum_required(VERSION 3.16)

project(fdk-aac)

set(AACDEC_SRC
  libAACdec/src/FDK_delay.cpp
  libAACdec/src/aac_ram.cpp
  libAACdec/src/aac_rom.cpp
  libAACdec/src/aacdec_drc.cpp
  libAACdec/src/aacdec_pns.cpp
  libAACdec/src/aacdec_tns.cpp
  libAACdec/src/aacdecoder.cpp
  libAACdec/src/aacdecoder_lib.cpp
  libAACdec/src/block.cpp
  libAACdec/src/channel.cpp
  libAACdec/src/channelinfo.cpp
  libAACdec/src/ldfiltbank.cpp
  libAACdec/src/pulsedata.cpp
  libAACdec/src/stereo.cpp
  libAACdec/src/aac_ram.h
  libAACdec/src/aac_rom.h
  libAACdec/src/aacdec_drc.h
  libAACdec/src/aacdec_drc_types.h
  libAACdec/src/aacdec_pns.h
  libAACdec/src/aacdec_tns.h
  libAACdec/src/aacdecoder.h
  libAACdec/src/block.h
  libAACdec/src/channel.h
  libAACdec/src/channelinfo.h
  libAACdec/src/FDK_delay.h
  libAACdec/src/ldfiltbank.h
  libAACdec/src/overlapadd.h
  libAACdec/src/pulsedata.h
  libAACdec/src/stereo.h)

set(AACENC_SRC
  libAACenc/src/aacEnc_ram.cpp
  libAACenc/src/aacEnc_rom.cpp
  libAACenc/src/aacenc.cpp
  libAACenc/src/aacenc_lib.cpp
  libAACenc/src/aacenc_pns.cpp
  libAACenc/src/aacenc_tns.cpp
  libAACenc/src/adj_thr.cpp
  libAACenc/src/band_nrg.cpp
  libAACenc/src/bandwidth.cpp
  libAACenc/src/bit_cnt.cpp
  libAACenc/src/bitenc.cpp
  libAACenc/src/block_switch.cpp
  libAACenc/src/channel_map.cpp
  libAACenc/src/chaosmeasure.cpp
  libAACenc/src/dyn_bits.cpp
  libAACenc/src/grp_data.cpp
  libAACenc/src/intensity.cpp
  libAACenc/src/line_pe.cpp
  libAACenc/src/metadata_compressor.cpp
  libAACenc/src/metadata_main.cpp
  libAACenc/src/ms_stereo.cpp
  libAACenc/src/noisedet.cpp
  libAACenc/src/pnsparam.cpp
  libAACenc/src/pre_echo_control.cpp
  libAACenc/src/psy_configuration.cpp
  libAACenc/src/psy_main.cpp
  libAACenc/src/qc_main.cpp
  libAACenc/src/quantize.cpp
  libAACenc/src/sf_estim.cpp
  libAACenc/src/spreading.cpp
  libAACenc/src/tonality.cpp
  libAACenc/src/transform.cpp
  libAACenc/src/aacenc.h
  libAACenc/src/aacenc_pns.h
  libAACenc/src/aacEnc_ram.h
  libAACenc/src/aacEnc_rom.h
  libAACenc/src/aacenc_tns.h
  libAACenc/src/adj_thr.h
  libAACenc/src/adj_thr_data.h
  libAACenc/src/band_nrg.h
  libAACenc/src/bandwidth.h
  libAACenc/src/bit_cnt.h
  libAACenc/src/bitenc.h
  libAACenc/src/block_switch.h
  libAACenc/src/channel_map.h
  libAACenc/src/chaosmeasure.h
  libAACenc/src/dyn_bits.h
  libAACenc/src/grp_data.h
  libAACenc/src/intensity.h
  libAACenc/src/interface.h
  libAACenc/src/line_pe.h
  libAACenc/src/metadata_compressor.h
  libAACenc/src/metadata_main.h
  libAACenc/src/ms_stereo.h
  libAACenc/src/noisedet.h
  libAACenc/src/pns_func.h
  libAACenc/src/pnsparam.h
  libAACenc/src/pre_echo_control.h
  libAACenc/src/psy_configuration.h
  libAACenc/src/psy_const.h
  libAACenc/src/psy_data.h
  libAACenc/src/psy_main.h
  libAACenc/src/qc_data.h
  libAACenc/src/qc_main.h
  libAACenc/src/quantize.h
  libAACenc/src/sf_estim.h
  libAACenc/src/spreading.h
  libAACenc/src/tns_func.h
  libAACenc/src/tonality.h
  libAACenc/src/transform.h)

set(ARITHCODING_SRC
  libArithCoding/src/ac_arith_coder.cpp)

set(DRCDEC_SRC
  libDRCdec/src/FDK_drcDecLib.cpp
  libDRCdec/src/drcDec_gainDecoder.cpp
  libDRCdec/src/drcDec_reader.cpp
  libDRCdec/src/drcDec_rom.cpp
  libDRCdec/src/drcDec_selectionProcess.cpp
  libDRCdec/src/drcDec_tools.cpp
  libDRCdec/src/drcGainDec_init.cpp
  libDRCdec/src/drcGainDec_preprocess.cpp
  libDRCdec/src/drcGainDec_process.cpp
  libDRCdec/src/drcDec_gainDecoder.h
  libDRCdec/src/drcDec_reader.h
  libDRCdec/src/drcDec_rom.h
  libDRCdec/src/drcDec_selectionProcess.h
  libDRCdec/src/drcDec_tools.h
  libDRCdec/src/drcDec_types.h
  libDRCdec/src/drcDecoder.h
  libDRCdec/src/drcGainDec_init.h
  libDRCdec/src/drcGainDec_preprocess.h
  libDRCdec/src/drcGainDec_process.h)

set(FDK_SRC
  libFDK/src/FDK_bitbuffer.cpp
  libFDK/src/FDK_core.cpp
  libFDK/src/FDK_crc.cpp
  libFDK/src/FDK_decorrelate.cpp
  libFDK/src/FDK_hybrid.cpp
  libFDK/src/FDK_lpc.cpp
  libFDK/src/FDK_matrixCalloc.cpp
  libFDK/src/FDK_qmf_domain.cpp
  libFDK/src/FDK_tools_rom.cpp
  libFDK/src/FDK_trigFcts.cpp
  libFDK/src/autocorr2nd.cpp
  libFDK/src/dct.cpp
  libFDK/src/fft.cpp
  libFDK/src/fft_rad2.cpp
  libFDK/src/fixpoint_math.cpp
  libFDK/src/huff_nodes.cpp
  libFDK/src/mdct.cpp
  libFDK/src/nlc_dec.cpp
  libFDK/src/qmf.cpp
  libFDK/src/scale.cpp)

set(MPEGTPDEC_SRC
  libMpegTPDec/src/tpdec_adif.cpp
  libMpegTPDec/src/tpdec_adts.cpp
  libMpegTPDec/src/tpdec_asc.cpp
  libMpegTPDec/src/tpdec_drm.cpp
  libMpegTPDec/src/tpdec_latm.cpp
  libMpegTPDec/src/tpdec_lib.cpp
  libMpegTPDec/src/tp_version.h
  libMpegTPDec/src/tpdec_adif.h
  libMpegTPDec/src/tpdec_adts.h
  libMpegTPDec/src/tpdec_drm.h
  libMpegTPDec/src/tpdec_latm.h)

set(MPEGTPENC_SRC
  libMpegTPEnc/src/tpenc_adif.cpp
  libMpegTPEnc/src/tpenc_adts.cpp
  libMpegTPEnc/src/tpenc_asc.cpp
  libMpegTPEnc/src/tpenc_latm.cpp
  libMpegTPEnc/src/tpenc_lib.cpp
  libMpegTPEnc/src/tp_version.h
  libMpegTPEnc/src/tpenc_adif.h
  libMpegTPEnc/src/tpenc_adts.h
  libMpegTPEnc/src/tpenc_asc.h
  libMpegTPEnc/src/tpenc_latm.h)

set(PCMUTILS_SRC
  libPCMutils/src/limiter.cpp
  libPCMutils/src/pcm_utils.cpp
  libPCMutils/src/pcmdmx_lib.cpp
  libPCMutils/src/version.h)

set(SYS_SRC
  libSYS/src/genericStds.cpp
  libSYS/src/syslib_channelMapDescr.cpp)

set(FDK_AAC_INCLUDE_HEADERS
  libSYS/include/machine_type.h
  libSYS/include/genericStds.h
  libSYS/include/FDK_audio.h
  libSYS/include/syslib_channelMapDescr.h
  libAACenc/include/aacenc_lib.h
  libAACdec/include/aacdecoder_lib.h)

set(FDK_AAC_SOURCES
  ${AACDEC_SRC} ${AACENC_SRC}
  ${ARITHCODING_SRC}
  ${DRCDEC_SRC}
  ${MPEGTPDEC_SRC} ${MPEGTPENC_SRC}
  ${PCMUTILS_SRC} ${FDK_SRC} ${SYS_SRC})

add_library(fdk-aac ${FDK_AAC_SOURCES})

### Library target includes
target_include_directories(fdk-aac
  PUBLIC
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libAACdec/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libAACenc/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libSYS/include>
  PRIVATE
  libArithCoding/include
  libDRCdec/include
  libMpegTPDec/include
  libMpegTPEnc/include
  libFDK/include
  libPCMutils/include)

target_compile_options(fdk-aac PRIVATE -fno-exceptions -fno-rtti -Wno-deprecated-enum-enum-conversion)
