#include "bell/audio/Container.h"

#include "ogg.h"

namespace bell::audio {
class OggContainer : public Container {
 public:
  OggContainer() = default;
  ~OggContainer() override{};

  // Base class overrides
  bell::Result<> openForRead(
      std::shared_ptr<bell::io::DataStream> dataStream) override;
  bell::Result<EncodedAudioFrame> readNextFrame() override;
  bell::Result<> seekToTime(uint64_t timeMs) override { return {}; }
  bell::Result<> seekToFrame(size_t frameIndex) override { return {}; }
  uint64_t tellTime() const override { return 0; }
  uint64_t getDurationMs() const override { return 0; }

 private:
  const char* LOG_TAG = "OggContainer";

  std::shared_ptr<bell::io::DataStream> stream;
  ogg_stream_state* oggStreamState;
  ogg_sync_state* oggSyncState;
  ogg_page oggPage;
  ogg_packet packet;
  uint32_t streamSerialNo;  // TODO: support multiple streams

  bell::Result<> readNextPage();

  void close();
};
};  // namespace bell::audio
