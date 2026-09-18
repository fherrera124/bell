#include <doctest/doctest.h>

#include <condition_variable>
#include <mutex>
#include <sys/socket.h>

#include "bell/Logger.h"
#include "bell/http/Server.h"

namespace {
struct LogState {
  std::mutex mutex;
  std::condition_variable changed;
  size_t closed = 0;
  std::vector<std::string> errors;
};
class Capture : public bell::LoggerBackend {
 public:
  LogState& state;
  explicit Capture(LogState& state) : state(state) {}
  void log(bell::LogLevel level, std::string_view, int, std::string_view,
           std::string_view message) override {
    std::lock_guard lock(state.mutex);
    if (level == bell::LogLevel::error) state.errors.emplace_back(message);
    if (message == "Closing connection") ++state.closed;
    state.changed.notify_all();
  }
};
struct LogCapture {
  LogState state;
  bell::LoggerBackend* backend;
  LogCapture() {
    auto capture = std::make_unique<Capture>(state);
    backend = capture.get();
    bell::registerLoggerBackend(std::move(capture));
  }
  ~LogCapture() { bell::unregisterLoggerBackend(backend); }
  bool waitClosed(size_t count) {
    std::unique_lock lock(state.mutex);
    return state.changed.wait_for(lock, std::chrono::seconds(3),
                                  [&] { return state.closed >= count; });
  }
};
}

TEST_CASE("Real socket timeout is retained after partially received body bytes") {
  int fds[2];
  REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  auto socket = std::make_shared<bell::net::TCPSocket>(fds[0]);
  bell::net::TCPSocket peer(fds[1]);
  REQUIRE(socket->setReceiveTimeout(30));
  const std::string wire = "HTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\nabc";
  REQUIRE(peer.writeAll(reinterpret_cast<const std::byte*>(wire.data()), wire.size()));
  auto stream = std::make_shared<bell::SocketStream>(socket);
  bell::http::Reader reader(bell::http::Direction::Response, stream);
  REQUIRE(reader.readHeaders());
  std::array<std::byte, 16> body{};
  auto partial = reader.readBodyChunk(body.data(), body.size());
  REQUIRE(partial);
  CHECK(*partial == 3);
  auto failed = reader.readBodyChunk(body.data(), body.size());
  REQUIRE_FALSE(failed);
  CHECK(failed.error() == std::errc::timed_out);
}

TEST_CASE("Real server handles keep-alive close quietly and logs a truncated header once") {
  LogCapture log;
  bell::http::Server server;
  server.registerGet("/", [](const auto&, const auto& writer, const auto&) {
    (void)writer->writeResponseWithBody(200, {}, "abc");
  });
  int port = 0;
  bool listening = false;
  for (int attempt = 0; attempt < 10 && !listening; ++attempt) {
    bell::net::TCPSocket reservation;
    auto bound = reservation.bind("127.0.0.1", 0);
    REQUIRE(bound);
    port = *bound;
    reservation.close();
    listening = bool(server.listen(port));
  }
  REQUIRE(listening);
  {
    bell::net::TCPSocket empty;
    REQUIRE(empty.connect("127.0.0.1", port, 1000));
  }
  REQUIRE(log.waitClosed(1));
  {
    auto socket = std::make_shared<bell::net::TCPSocket>();
    REQUIRE(socket->connect("127.0.0.1", port, 1000));
    REQUIRE(socket->setReceiveTimeout(1000));
    auto stream = std::make_shared<bell::SocketStream>(socket);
    for (int i = 0; i < 2; ++i) {
      *stream << "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" << std::flush;
      bell::http::Reader response(bell::http::Direction::Response, stream);
      REQUIRE(response.readHeaders());
      auto body = response.getBodyStringView();
      REQUIRE(body);
      CHECK(*body == "abc");
    }
  }
  REQUIRE(log.waitClosed(2));
  {
    std::lock_guard lock(log.state.mutex);
    CHECK(log.state.errors.empty());
  }
  {
    bell::net::TCPSocket socket;
    REQUIRE(socket.connect("127.0.0.1", port, 1000));
    const std::string partial = "GET / HTTP/1.1\r\nHost:";
    REQUIRE(socket.writeAll(reinterpret_cast<const std::byte*>(partial.data()), partial.size()));
    REQUIRE(::shutdown(socket.getFd(), SHUT_WR) == 0);
    REQUIRE(log.waitClosed(3));
  }
  {
    std::lock_guard lock(log.state.mutex);
    REQUIRE(log.state.errors.size() == 1);
    CHECK(log.state.errors.front().find("Incomplete HTTP message") != std::string::npos);
  }
}
