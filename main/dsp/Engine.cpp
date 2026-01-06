#include "bell/dsp/Engine.h"
#include "bell/audio/Common.h"

// Enable profiling with -DBELL_DSP_ENABLE_PROFILING
#ifdef BELL_DSP_ENABLE_PROFILING
#include "bell/Logger.h"
#include "bell/utils/ClockCounter.h"
#endif


using namespace bell::dsp;

void Engine::applyPipeline(const std::shared_ptr<TransformPipeline>& pipeline) {
  // Replace the active pipeline with the new one
  std::scoped_lock lock(accessMutex);
  activePipeline = pipeline;
}

DataSlots* Engine::process(const std::byte* inputBuffer, size_t inputBufferLen,
                           std::byte* outputBuffer, size_t outputBufferLen,
                           const audio::Format& format) {
#ifdef BELL_DSP_ENABLE_PROFILING
  auto processStartTime = bell::utils::ClockCounter::now();
#endif

  std::scoped_lock lock(accessMutex);

  // Check if a pipeline is set
  if (!activePipeline) {
    return nullptr;
  }

  size_t numSamples = format.bytesToSamples(inputBufferLen);

  // Configure data slots if format changed (lazy allocation)
  if (innerDataSlots.sampleFormat != format ||
      innerDataSlots.numSamples != numSamples) {
    innerDataSlots.configure(numSamples, format);
  }

  const auto* inputAsInt16 = reinterpret_cast<const int16_t*>(inputBuffer);
  const auto* inputAsInt32 = reinterpret_cast<const int32_t*>(inputBuffer);

  uint8_t numChannels = format.getNumChannels();

#ifdef BELL_DSP_ENABLE_PROFILING
  auto inputConversionStart = bell::utils::ClockCounter::now();
#endif

  switch (format.getSampleFormat()) {
    case audio::SampleFormat::S16: {
      if (numChannels == 2) {
        // SIMD-lite optimization for stereo: read 32-bit word to get both L+R samples
        const auto* inputAs32 = reinterpret_cast<const uint32_t*>(inputAsInt16);
        int32_t* leftChan = (*innerDataSlots.primarySlot)[0];
        int32_t* rightChan = (*innerDataSlots.primarySlot)[1];

        for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
             frameIdx++) {
          uint32_t stereoFrame = inputAs32[frameIdx];
          // Extract left (lower 16 bits) and right (upper 16 bits)
          int16_t left = static_cast<int16_t>(stereoFrame & 0xFFFF);
          int16_t right = static_cast<int16_t>(stereoFrame >> 16);
          leftChan[frameIdx] = static_cast<int32_t>(left) << 15U;
          rightChan[frameIdx] = static_cast<int32_t>(right) << 15U;
        }
      } else {
        const int16_t* src = inputAsInt16;
        for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
             frameIdx++) {
          for (size_t chan = 0; chan < numChannels; chan++) {
            (*innerDataSlots.primarySlot)[chan][frameIdx] =
                static_cast<int32_t>(*src++) << 15U;
          }
        }
      }
      break;
    }

    case audio::SampleFormat::S32: {
      if (numChannels == 2) {
        // Optimized stereo path
        const int32_t* src = inputAsInt32;
        int32_t* leftChan = (*innerDataSlots.primarySlot)[0];
        int32_t* rightChan = (*innerDataSlots.primarySlot)[1];

        for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
             frameIdx++) {
          leftChan[frameIdx] = *src++;
          rightChan[frameIdx] = *src++;
        }
      } else {
        // General case
        const int32_t* src = inputAsInt32;
        for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
             frameIdx++) {
          for (size_t chan = 0; chan < numChannels; chan++) {
            (*innerDataSlots.primarySlot)[chan][frameIdx] = *src++;
          }
        }
      }
      break;
    }

    default:
      // Unsupported bit width
      throw std::runtime_error("Unsupported bit width");
      break;
  }

#ifdef BELL_DSP_ENABLE_PROFILING
  auto inputConversionEnd = bell::utils::ClockCounter::now();
  stats.inputConversionCycles += bell::utils::ClockCounter::elapsed(
      inputConversionStart, inputConversionEnd);
#endif

  // Process the samples
#ifdef BELL_DSP_ENABLE_PROFILING
  auto pipelineStart = bell::utils::ClockCounter::now();
#endif
  activePipeline->process(innerDataSlots);
#ifdef BELL_DSP_ENABLE_PROFILING
  auto pipelineEnd = bell::utils::ClockCounter::now();
  stats.pipelineProcessingCycles +=
      bell::utils::ClockCounter::elapsed(pipelineStart, pipelineEnd);
#endif

  // Check if the output buffer is large enough
  if (format.samplesToBytes(innerDataSlots.numSamples) > outputBufferLen) {
    throw std::runtime_error("Output buffer is too small");
    return nullptr;
  }

  auto* outputData16 = reinterpret_cast<int16_t*>(outputBuffer);
  auto* outputData32 = reinterpret_cast<int32_t*>(outputBuffer);

#ifdef BELL_DSP_ENABLE_PROFILING
  auto outputConversionStart = bell::utils::ClockCounter::now();
#endif

  switch (format.getSampleFormat()) {
    case audio::SampleFormat::S16: {
      if (numChannels == 2) {
        // SIMD-lite optimization for stereo: write 32-bit word with both L+R samples
        auto* outputAs32 = reinterpret_cast<uint32_t*>(outputData16);
        const int32_t* leftChan = (*innerDataSlots.primarySlot)[0];
        const int32_t* rightChan = (*innerDataSlots.primarySlot)[1];

        for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
             frameIdx++) {
          int16_t left = static_cast<int16_t>(leftChan[frameIdx] >> 15U);
          int16_t right = static_cast<int16_t>(rightChan[frameIdx] >> 15U);
          // Pack both samples into a single 32-bit write
          uint32_t stereoFrame =
              (static_cast<uint32_t>(static_cast<uint16_t>(left))) |
              (static_cast<uint32_t>(static_cast<uint16_t>(right)) << 16);
          outputAs32[frameIdx] = stereoFrame;
        }
      } else {
        // General case: cache-friendly loop order
        int16_t* dst = outputData16;
        for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
             frameIdx++) {
          for (size_t chan = 0; chan < numChannels; chan++) {
            *dst++ = static_cast<int16_t>(
                (*innerDataSlots.primarySlot)[chan][frameIdx] >> 15U);
          }
        }
      }
      break;
    }

    case audio::SampleFormat::S32: {
      // Cache-friendly sequential writes
      if (numChannels == 2) {
        // Optimized stereo path
        int32_t* dst = outputData32;
        const int32_t* leftChan = (*innerDataSlots.primarySlot)[0];
        const int32_t* rightChan = (*innerDataSlots.primarySlot)[1];

        for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
             frameIdx++) {
          *dst++ = leftChan[frameIdx];
          *dst++ = rightChan[frameIdx];
        }
      } else {
        // General case
        int32_t* dst = outputData32;
        for (size_t frameIdx = 0; frameIdx < innerDataSlots.numSamples;
             frameIdx++) {
          for (size_t chan = 0; chan < numChannels; chan++) {
            *dst++ = (*innerDataSlots.primarySlot)[chan][frameIdx];
          }
        }
      }
      break;
    }

    default:
      // Unsupported bit width
      throw std::runtime_error("Unsupported bit width");
      break;
  }

#ifdef BELL_DSP_ENABLE_PROFILING
  auto outputConversionEnd = bell::utils::ClockCounter::now();
  stats.outputConversionCycles += bell::utils::ClockCounter::elapsed(
      outputConversionStart, outputConversionEnd);

  auto processEndTime = bell::utils::ClockCounter::now();
  stats.totalCycles +=
      bell::utils::ClockCounter::elapsed(processStartTime, processEndTime);
  stats.callCount++;

  // Periodic logging of profiling results
  auto currentTime = bell::utils::ClockCounter::now();
  uint64_t timeSinceLastLog = bell::utils::ClockCounter::toMilliseconds(
      bell::utils::ClockCounter::elapsed(lastLogTime, currentTime));

  if (lastLogTime == 0 || timeSinceLastLog >= LOG_INTERVAL_MS) {
    lastLogTime = currentTime;

    if (stats.callCount > 0) {
      uint64_t avgInputUs = bell::utils::ClockCounter::toMicroseconds(
          stats.inputConversionCycles / stats.callCount);
      uint64_t avgPipelineUs = bell::utils::ClockCounter::toMicroseconds(
          stats.pipelineProcessingCycles / stats.callCount);
      uint64_t avgOutputUs = bell::utils::ClockCounter::toMicroseconds(
          stats.outputConversionCycles / stats.callCount);
      uint64_t avgTotalUs = bell::utils::ClockCounter::toMicroseconds(
          stats.totalCycles / stats.callCount);

      BELL_LOG(info, "DSP",
               "Engine Profiling (avg over {} calls):", stats.callCount);
      BELL_LOG(info, "DSP", "  Input conversion:  {} us", avgInputUs);
      BELL_LOG(info, "DSP", "  Pipeline process:  {} us", avgPipelineUs);
      BELL_LOG(info, "DSP", "  Output conversion: {} us", avgOutputUs);
      BELL_LOG(info, "DSP", "  Total:             {} us", avgTotalUs);

      // Reset statistics after logging
      stats = ProfilingStats{};
    }
  }
#endif

  return &innerDataSlots;
}
