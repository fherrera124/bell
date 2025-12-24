#include "bell/Logger.h"

// Needed for thread-safe time handling
#include <chrono>

using namespace bell;

StdoutLoggerBackend::StdoutLoggerBackend(bool includeTags,
                                         bool logFullTimestamp)
    : includeTags(includeTags), logFullTimestamp(logFullTimestamp) {}

void StdoutLoggerBackend::log(LogLevel level, std::string_view filename,
                              int line, std::string_view tag,
                              std::string_view message) {
  if (level < logLevel.load(std::memory_order_relaxed)) {
    return;
  }

  fmt::memory_buffer out;

  auto now = std::chrono::system_clock::now();
  auto tNow = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  if (logFullTimestamp) {
    // Format: YYYY-MM-DD HH:MM:SS.ms
    fmt::format_to(std::back_inserter(out), "[{:%Y-%m-%d %H:%M:%S}.{:03}] ",
                   fmt::localtime(tNow), ms.count());
  } else {
    // Format: HH:MM:SS.ms
    fmt::format_to(std::back_inserter(out), "[{:%H:%M:%S}.{:03}] ",
                   fmt::localtime(tNow), ms.count());
  }

  switch (level) {
    case LogLevel::debug:
      fmt::format_to(std::back_inserter(out), fg(fmt::color::dark_gray), "D ");
      break;
    case LogLevel::info:
      fmt::format_to(std::back_inserter(out), fg(fmt::color::blue), "I ");
      break;
    case LogLevel::warn:
      fmt::format_to(std::back_inserter(out), fg(fmt::color::yellow), "W ");
      break;
    case LogLevel::error:
      fmt::format_to(std::back_inserter(out), fg(fmt::color::red), "E ");
      break;
  }

  if (includeTags && !tag.empty()) {
    fmt::format_to(std::back_inserter(out), "[{}] ", tag);
  }

  // Get color for a filename
  unsigned long hash = 5381;
  for (char const& c : filename) {
    hash = ((hash << 5) + hash) + c;  // hash * 33 + c
  }

  // Apply calculated color to filename
  uint8_t colorCode = allColors[hash % allColors.size()];

  fmt::format_to(std::back_inserter(out), "\033[0;{}m{}:\033[0m{}: ", colorCode,
                 filename, line);

  if (level == LogLevel::error) {
    fmt::format_to(std::back_inserter(out), fg(fmt::color::red), "{}\n",
                   message);
  } else if (level == LogLevel::warn) {
    fmt::format_to(std::back_inserter(out), fg(fmt::color::yellow), "{}\n",
                   message);
  } else {
    fmt::format_to(std::back_inserter(out), "{}\n", message);
  }

  fmt::print(stdout, "{}", fmt::to_string(out));
}
