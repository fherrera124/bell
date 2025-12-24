#pragma once

// Standard includes
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

// Library includes
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>

namespace std {
inline auto format_as(std::error_code err) {
  return err.message();
}
}  // namespace std

namespace bell {

enum class LogLevel { debug, info, warn, error };

class LoggerBackend {
 public:
  LoggerBackend() = default;
  virtual ~LoggerBackend() = default;

  // Use string_view to avoid allocations for filename and tag
  virtual void log(LogLevel level, std::string_view filename, int line,
                   std::string_view tag, std::string_view message) = 0;

  void setLogLevel(LogLevel level) {
    logLevel.store(level, std::memory_order_relaxed);
  }

  LogLevel getLogLevel() const {
    return logLevel.load(std::memory_order_relaxed);
  }

 protected:
  std::atomic<LogLevel> logLevel{LogLevel::debug};
};

class BaseLogger {
 private:
  std::vector<std::unique_ptr<LoggerBackend>> registeredBackends;

  // Use shared_mutex: allow multiple loggers (readers) at once,
  // block only when adding backends (writer)
  mutable std::shared_mutex loggerMutex;

  // Keep track of the lowest active level globally
  std::atomic<LogLevel> globalMinLevel{LogLevel::debug};

  void updateGlobalMinLevel() {
    // Must be called under lock
    LogLevel min = LogLevel::error;
    for (const auto& b : registeredBackends) {
      if (b->getLogLevel() < min)
        min = b->getLogLevel();
    }
    globalMinLevel.store(min, std::memory_order_relaxed);
  }

 public:
  BaseLogger() = default;
  ~BaseLogger() = default;

  static BaseLogger& instance() {
    static BaseLogger logger;
    return logger;
  }

  // Fast path check to be used by the Macro
  inline bool shouldLog(LogLevel level) const {
    return level >= globalMinLevel.load(std::memory_order_relaxed);
  }

  void registerBackend(std::unique_ptr<LoggerBackend> logger) {
    std::unique_lock lock(loggerMutex);
    // Update the global min level if this new logger is more verbose
    if (logger->getLogLevel() < globalMinLevel.load()) {
      globalMinLevel.store(logger->getLogLevel());
    }
    registeredBackends.push_back(std::move(logger));
    updateGlobalMinLevel();
  }

  void setLogLevel(LogLevel level) {
    std::unique_lock lock(loggerMutex);
    for (auto& backend : registeredBackends) {
      backend->setLogLevel(level);
    }
    updateGlobalMinLevel();
  }

  // Helper to extract basename without allocation
  static constexpr std::string_view getBasename(std::string_view path) {
    auto lastSlash = path.find_last_of("/\\");
    if (lastSlash == std::string_view::npos)
      return path;
    return path.substr(lastSlash + 1);
  }

  // Generic log entry point
  template <typename... Args>
  void log(LogLevel level, std::string_view filename, int line,
           std::string_view tag, fmt::format_string<Args...> format,
           Args&&... args) {
    std::string msg;
    try {
      msg = fmt::format(format, std::forward<Args>(args)...);
    } catch (const std::exception& e) {
      msg = fmt::format("LOGGING ERROR: {}", e.what());
    }

    auto basename = getBasename(filename);
    std::shared_lock lock(loggerMutex);

    for (const auto& backend : registeredBackends) {
      // 4. Double check backend specific level
      if (level >= backend->getLogLevel()) {
        backend->log(level, basename, line, tag, msg);
      }
    }
  }
};

class StdoutLoggerBackend : public bell::LoggerBackend {
 public:
  StdoutLoggerBackend(bool includeTags, bool logFullTimestamp);

  void log(LogLevel level, std::string_view filename, int line,
           std::string_view tag, std::string_view message) override;

 private:
  bool includeTags;
  bool logFullTimestamp;
  static constexpr std::array<uint8_t, 15> allColors = {
      31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97};
};

/**
 * @brief Registers the Stdout logger (Helper function)
 */
inline void registerDefaultLogger(bool includeTags = false,
                                  bool logFullTimestamp = false,
                                  LogLevel level = LogLevel::debug) {
  auto backend =
      std::make_unique<StdoutLoggerBackend>(includeTags, logFullTimestamp);
  backend->setLogLevel(level);
  BaseLogger::instance().registerBackend(std::move(backend));
}

/**
 * @brief Registers a logger implementation.
 */
inline void registerLoggerBackend(std::unique_ptr<LoggerBackend> backend) {
  BaseLogger::instance().registerBackend(std::move(backend));
}

/**
 * @brief Set the minimum log level globally
 */
inline void setLogLevel(LogLevel level) {
  BaseLogger::instance().setLogLevel(level);
}
}  // namespace bell

#define BELL_LOG(type_name, tag, ...)                                        \
  do {                                                                       \
    if (bell::BaseLogger::instance().shouldLog(bell::LogLevel::type_name)) { \
      bell::BaseLogger::instance().log(bell::LogLevel::type_name, __FILE__,  \
                                       __LINE__, tag, __VA_ARGS__);          \
    }                                                                        \
  } while (0)
