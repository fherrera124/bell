#include <filesystem>
#include <fstream>
#include "bell/Logger.h"
#include "bell/audio/OggContainer.h"
#include "bell/audio/TremorVorbisCodec.h"
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

   res = oggContainer->seekToFrame(20000);
   if (!res) {
     std::cerr << "Failed to seek to frame" << std::endl;
     std::cout << "Error message: " << res.error().message() << std::endl;
     return 1;
   }

   auto frameRes = oggContainer->readNextPacket();
   if (!frameRes) {
     std::cerr << "Failed to read packet" << std::endl;
     return 1;
   }

   std::cout << frameRes->data.size() << std::endl;
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
