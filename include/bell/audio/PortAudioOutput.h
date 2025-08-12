// #pragma once

// // Only include this file if the Portaudio backend is enabled
// #ifdef BELL_BACKEND_PORTAUDIO

// // Standard includes
// #include <mutex>

// // bell includes
// #include "bell/audio/Common.h"
// #include "bell/audio/OutputBackend.h"

// // Library includes
// #include <portaudio.h>

// namespace bell::audio {

// namespace internal {
// // Map PortAudio error codes to std::error_code
// struct portaudio_error_category : public std::error_category {
//   const char* name() const noexcept override { return "PortAudio"; }
//   std::string message(int ev) const noexcept override {
//     switch (static_cast<PaErrorCode>(ev)) {
//       case paNoError:
//         return "No Error.";
//       case paNotInitialized:
//         return "PortAudio not initialized.";
//       case paUnanticipatedHostError:
//         return "An unanticipated host error has occurred.";
//       case paInvalidChannelCount:
//         return "Invalid number of channels.";
//       case paInvalidSampleRate:
//         return "Invalid sample rate.";
//       case paInvalidDevice:
//         return "Invalid device.";
//       case paInvalidFlag:
//         return "Invalid flag.";
//       case paSampleFormatNotSupported:
//         return "Sample format not supported.";
//       case paBadIODeviceCombination:
//         return "Invalid combination of input and output devices.";
//       case paInsufficientMemory:
//         return "Insufficient memory.";
//       case paBufferTooBig:
//         return "Buffer too big.";
//       case paBufferTooSmall:
//         return "Buffer too small.";
//       case paNullCallback:
//         return "Callback function is null.";
//       case paBadStreamPtr:
//         return "Invalid stream pointer.";
//       case paTimedOut:
//         return "Operation timed out.";
//       case paInternalError:
//         return "An internal error occurred.";
//       case paDeviceUnavailable:
//         return "Device is unavailable.";
//       case paIncompatibleHostApiSpecificStreamInfo:
//         return "Incompatible host API-specific stream info.";
//       case paStreamIsStopped:
//         return "Stream is stopped.";
//       case paStreamIsNotStopped:
//         return "Stream is not stopped.";
//       case paInputOverflowed:
//         return "Input overflowed.";
//       case paOutputUnderflowed:
//         return "Output underflowed.";
//       case paHostApiNotFound:
//         return "Host API not found.";
//       case paInvalidHostApi:
//         return "Invalid Host API.";
//       case paCanNotReadFromACallbackStream:
//         return "Cannot read from a callback stream.";
//       case paCanNotWriteToACallbackStream:
//         return "Cannot write to a callback stream.";
//       case paCanNotReadFromAnOutputOnlyStream:
//         return "Cannot read from an output-only stream.";
//       case paCanNotWriteToAnInputOnlyStream:
//         return "Cannot write to an input-only stream.";
//       case paIncompatibleStreamHostApi:
//         return "Incompatible stream host API.";
//       case paBadBufferPtr:
//         return "Invalid buffer pointer.";
//       default:
//         return "Unknown PortAudio error";
//     }
//   }
// };
// }  // namespace internal

// // std::error_code helper
// inline std::error_code make_portaudio_error_code(int err) {
//   return {static_cast<int>(err), internal::portaudio_error_category()};
// };

// /**
//  * @brief Audio output backend using the Portaudio library
//  */
// class PortAudioOutput : public OutputBackend {
//  public:
//   PortAudioOutput() = default;
//   ~PortAudioOutput() override;

//   struct PortAudioOutputConfig : public OutputBackendConfig {
//     // Portaudio specific configuration
//     int deviceIndex = -1;  // Device index to use, -1 for default
//     int framesPerBuffer =
//         1024;  // Frames per buffer, 0 for paFramesPerBufferUnspecified
//     double suggestedLatency = 0.000;  // Suggested latency in seconds
//   };

//   // Delete copy constructor and copy assignment operator
//   PortAudioOutput(const PortAudioOutput&) = delete;
//   PortAudioOutput& operator=(const PortAudioOutput&) = delete;

//   // Output implementation
//   void configure(const std::any& outputConfig) override;
//   uint32_t write(const uint8_t* pcmData, size_t length) override;

//  private:
//   const char* LOG_TAG = "PortAudioBackend";
//   // Portaudio stream
//   void* stream = nullptr;
//   std::mutex configMutex;

//   // Flag to check if Portaudio is initialized
//   bool portaudioInitialized = false;

//   // Configured audio format
//   audio::Format audioFormat;
// };
// }  // namespace bell::audio

// namespace bell {
// // Type alias for the Portaudio audio output backend
// using PortAudioOutput = audio::PortAudioOutput;
// using PortAudioOutputConfig = audio::PortAudioOutput::PortAudioOutputConfig;
// };  // namespace bell

// #endif
