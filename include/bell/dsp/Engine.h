#pragma once

// Standard includes
#include <memory>
#include <mutex>

// Library includes
#include "TransformPipeline.h"

namespace bell::dsp {
class Engine {
 public:
  Engine() = default;
  ~Engine() = default;

  /**
   * @brief Set the pipeline to be used for processing audio samples. If a pipeline is already set, it will be replaced.
   *
   * @param pipeline Pointer to the pipeline to use
   */
  void applyPipeline(const std::shared_ptr<TransformPipeline>& pipeline);

  /**
   * @brief Process audio samples using the pipeline set by applyPipeline.
   *
   * @param inputBuffer Pointer to the input audio samples, in the format specified by format
   * @param inputBufferLen Length of the input buffer
   * @param outputBuffer Pointer to the output buffer, where the processed audio samples will be written. Must be large enough to hold the processed samples,
   *  and in case of the pipeline adding samples, must be large enough to hold the additional samples.
   * @param outputBufferLen Length of the output buffer
   * @param format Format of the audio samples
   * @return DataSlots* Pointer to the data slots containing the processed audio information. The caller should check whether the audio format has changed.
   */
  DataSlots* process(const std::byte* inputBuffer, size_t inputBufferLen,
                     std::byte* outputBuffer, size_t outputBufferLen,
                     const audio::Format& format);

 private:
  std::shared_ptr<TransformPipeline> activePipeline;
  DataSlots innerDataSlots;
  std::mutex accessMutex;

#ifdef BELL_DSP_ENABLE_PROFILING
  // Profiling state (only compiled when profiling is enabled)
  struct ProfilingStats {
    uint64_t inputConversionCycles = 0;
    uint64_t pipelineProcessingCycles = 0;
    uint64_t outputConversionCycles = 0;
    uint64_t totalCycles = 0;
    uint64_t callCount = 0;
  };
  ProfilingStats stats{};
  uint64_t lastLogTime = 0;
  static constexpr uint64_t LOG_INTERVAL_MS = 5000;  // Log every 5 seconds
#endif
};
}  // namespace bell::dsp

namespace bell {
using DspEngine = dsp::Engine;
}
