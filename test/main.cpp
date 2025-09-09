#include <filesystem>
#include <fstream>
#include "bell/Logger.h"
#include "bell/audio/OggContainer.h"
#include "bell/audio/VorbisCodec.h"
#include "bell/io/FileDataStream.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

int main(int argc, char* argv[]) {
  bell::registerDefaultLogger();
  (void)argc;
  (void)argv;

  std::cout << std::filesystem::current_path() << std::endl;

  std::ifstream file("dupa2.dec");
  if (!file.is_open()) {
    std::cerr << "Failed to open file" << std::endl;
    return 1;
  }

  if (!file.good()) {
    std::cerr << "Failed to read file" << std::endl;
    return 1;
  }

  auto fileStream = std::make_shared<bell::io::FileDataStream>(std::move(file));
  auto oggContainer = std::make_shared<bell::audio::OggContainer>();

  auto res = oggContainer->openForRead(fileStream);
  if (!res) {
    std::cerr << "Failed to open for read" << std::endl;
    return 1;
  }

  auto vorbisCodec = std::make_shared<bell::audio::VorbisCodec>();

  bool isSetup = false;
  for (int x = 0; x < 10; ++x) {
    auto pageRes = oggContainer->readNextFrame();
    if (!pageRes) {
      std::cerr << "Failed to read frame" << std::endl;

    } else if (!isSetup) {
      auto page = pageRes.value();
      std::cout << "Page size: " << page.data.size() << std::endl;
      auto setupRes = vorbisCodec->setupDecodeFromHeaders(page.data.data(),
                                                          page.data.size());
      if (setupRes.has_value() && setupRes.value()) {
        std::cout << "Setup successful" << std::endl;
        isSetup = true;
      } else if (setupRes.has_value()) {
        std::cerr << "Need more frame" << std::endl;
      } else {
        std::cerr << "Failed to setup decode" << setupRes.error().message()<< std::endl;
      }
    } else if (isSetup) {
      auto page = pageRes.value();
      std::cout << "Page size: " << page.data.size() << std::endl;
      size_t outputLen = 0;
      auto decodeRes =
          vorbisCodec->decode(page.data.data(), page.data.size(), outputLen);
      if (decodeRes.has_value()) {
        std::cout << "Decode successful " << outputLen << std::endl;
      } else {
        std::cerr << "Failed to decode" << decodeRes.error().message() << std::endl;
      }
    }
  }

  // return 0;

  // doctest::Context context;
  // context.applyCommandLine(argc, argv);

  // int res = context.run();  // run doctest

  // // important - query flags (and --exit) rely on the user doing this
  // if (context.shouldExit()) {
  //   return res;
  // }

  return 0;
}
