#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <sstream>
#include <variant>

#include "bell/http/Client.h"
#include "bell/http/DataStream.h"
#include "bell/net/TCPSocket.h"

namespace {
using namespace bell;
using namespace bell::http;
using Step = std::variant<std::string, std::error_code>;

class ScriptedSocket : public net::TCPSocket {
 public:
  std::deque<Step> steps;
  bool open = true;
  size_t calls = 0;
  explicit ScriptedSocket(std::deque<Step> steps) : steps(std::move(steps)) {}
  Result<> setBlocking(bool) override { return {}; }
  Result<> setReceiveTimeout(int) override { return {}; }
  Result<> setSendTimeout(int) override { return {}; }
  bool isValid() const override { return open; }
  void close() override { open = false; }
  Result<size_t> write(const std::byte*, size_t len) override { return len; }
  Result<size_t> read(std::byte* dst, size_t len) override {
    ++calls;
    if (steps.empty()) return size_t{0};
    if (auto* error = std::get_if<std::error_code>(&steps.front())) {
      auto copy = *error;
      steps.pop_front();
      return nonstd::make_unexpected(copy);
    }
    auto& bytes = std::get<std::string>(steps.front());
    const auto count = std::min(len, bytes.size());
    std::memcpy(dst, bytes.data(), count);
    bytes.erase(0, count);
    if (bytes.empty()) steps.pop_front();
    return count;
  }
};

std::string response(std::string body, size_t length) {
  return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(length) +
      "\r\n\r\n" + body;
}

const std::string get = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

std::shared_ptr<SocketStream> stream(std::deque<Step> steps) {
  return std::make_shared<SocketStream>(std::make_shared<ScriptedSocket>(std::move(steps)));
}

class ScriptedTransport : public Transport {
 public:
  std::deque<Step> steps;
  explicit ScriptedTransport(std::deque<Step> steps) : steps(std::move(steps)) {}
  Result<Response> execute(const Request&) override {
    Reader reader(Direction::Response, stream(std::move(steps)));
    auto res = reader.readHeaders();
    if (!res) return nonstd::make_unexpected(res.error());
    return Response(std::move(reader));
  }
};
}  // namespace

TEST_CASE("HTTP read termination: headers retain EOF and transport errors") {
  for (auto direction : {Direction::Request, Direction::Response}) {
    for (const auto& prefix : {std::string{}, std::string{"HTTP/"}}) {
      auto s = stream({prefix});
      Reader reader(direction, s.get());
      auto res = reader.readHeaders();
      REQUIRE_FALSE(res);
      CHECK(res.error() == (prefix.empty() ? Errc::EndOfStream : Errc::IncompleteMessage));
    }
    for (auto error : {std::errc::timed_out, std::errc::connection_reset}) {
      for (const auto& prefix : {std::string{}, std::string{"HTTP/"}}) {
        std::deque<Step> steps;
        if (!prefix.empty()) steps.emplace_back(prefix);
        steps.emplace_back(std::make_error_code(error));
        auto s = stream(std::move(steps));
        Reader reader(direction, s.get());
        auto res = reader.readHeaders();
        REQUIRE_FALSE(res);
        CHECK(res.error() == error);
      }
    }
  }
}

TEST_CASE("SocketStream retains partial reads and a sticky failure") {
  auto socket = std::make_shared<ScriptedSocket>(std::deque<Step>{
      std::make_error_code(std::errc::interrupted), std::string("abc"),
      std::make_error_code(std::errc::operation_would_block)});
  SocketStream s(socket);
  CHECK(s.totalBytesConsumed() == 0);
  char out[8]{};
  s.read(out, 8);
  CHECK(s.gcount() == 3);
  CHECK(std::string(out, 3) == "abc");
  CHECK(s.totalBytesConsumed() == 3);
  CHECK(s.readState() == net::ReadState::Error);
  CHECK(s.readError() == std::errc::timed_out);
  CHECK(socket->calls == 3);
  s.clear();
  CHECK(s.get() == std::char_traits<char>::eof());
  CHECK(socket->calls == 3);
  CHECK(s.readError() == std::errc::timed_out);
}

TEST_CASE("HTTP read termination: fragmented and buffered keep-alive requests") {
  std::deque<Step> bytes;
  for (char c : get) bytes.emplace_back(std::string(1, c));
  bytes.emplace_back(get + get);
  auto s = stream(std::move(bytes));
  std::vector<char> buffer;
  for (int i = 0; i < 3; ++i) {
    Reader reader(Direction::Request, s.get(), &buffer);
    REQUIRE(reader.readHeaders());
    CHECK(reader.getPath() == "/");
    CHECK(reader.discardRemainingBody());
  }
  Reader next(Direction::Request, s.get(), &buffer);
  auto res = next.readHeaders();
  REQUIRE_FALSE(res);
  CHECK(res.error() == Errc::EndOfStream);
}

TEST_CASE("HTTP body returns partial data before the original failure") {
  for (auto error : {std::error_code{}, std::make_error_code(std::errc::timed_out),
                     std::make_error_code(std::errc::connection_reset)}) {
    std::deque<Step> steps{response("abc", 6)};
    if (error) steps.emplace_back(error);
    auto s = stream(std::move(steps));
    Reader reader(Direction::Response, s);
    REQUIRE(reader.readHeaders());
    std::array<std::byte, 8> out{};
    auto first = reader.readBodyChunk(out.data(), out.size());
    REQUIRE(first);
    CHECK(*first == 3);
    CHECK(std::memcmp(out.data(), "abc", 3) == 0);
    CHECK_FALSE(s->isOpen());
    auto next = reader.readBodyChunk(out.data(), out.size());
    REQUIRE_FALSE(next);
    CHECK(next.error() == (error ? error : make_error_code(Errc::IncompleteMessage)));
    CHECK_FALSE(reader.discardRemainingBody());
  }
}

TEST_CASE("Buffered body access never turns a previous truncation into success") {
  Reader reader(Direction::Response, stream({response("abc", 8)}));
  REQUIRE(reader.readHeaders());
  CHECK_FALSE(reader.getBodyStringView());
  CHECK_FALSE(reader.getBodyBytes());
  CHECK_FALSE(reader.getBodyBytesPtr());
  CHECK_FALSE(reader.getBodyBytesLength());
}

TEST_CASE("HTTP body boundaries do not consume the next response") {
  auto s = stream({response("abc", 3) + response("def", 3)});
  for (const auto& expected : {"abc", "def"}) {
    Reader reader(Direction::Response, s);
    REQUIRE(reader.readHeaders());
    auto body = reader.getBodyStringView();
    REQUIRE(body);
    CHECK(*body == expected);
    CHECK(reader.getHeader("Content-Length") == "3");
  }
  CHECK(s->isOpen());
}

TEST_CASE("Reader moves preserve views and release the overwritten connection") {
  auto oldStream = stream({response("old", 3)});
  Reader target(Direction::Response, oldStream);
  REQUIRE(target.readHeaders());
  {
    Reader source(Direction::Response, stream({response(std::string(8192, 'a'), 8192)}));
    REQUIRE(source.readHeaders());
    target = std::move(source);
  }
  CHECK_FALSE(oldStream->isOpen());
  auto body = target.getBodyStringView();
  REQUIRE(body);
  CHECK(body->size() == 8192);
  CHECK(target.getStatusMessage() == "OK");
  CHECK(target.getHeader("Content-Length") == "8192");
  CHECK(target.keepAliveRequested());
}

TEST_CASE("Failed and undrained HTTP responses cannot return to the pool") {
  for (const auto& wire : {std::string{}, std::string{"HTTP/1.1 200 OK\r\n"},
                           response("short", 20),
                           std::string{"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"},
                           std::string{"HTTP/1.1 200 OK\r\nContent-Length: invalid\r\n\r\n"}}) {
    auto pool = std::make_shared<ConnectionPool>();
    pool->insert("localhost", 80, std::make_unique<ScriptedSocket>(std::deque<Step>{wire}));
    {
      auto lease = pool->acquire("localhost", 80);
      REQUIRE(lease);
      Reader reader(Direction::Response, std::make_shared<SocketStream>(*lease));
      auto headers = reader.readHeaders();
      if (headers) (void)reader.getBodyStringView();
    }
    CHECK_FALSE(pool->acquire("localhost", 80));
  }
}

TEST_CASE("Complete responses remain reusable after Reader moves") {
  auto pool = std::make_shared<ConnectionPool>();
  pool->insert("localhost", 80, std::make_unique<ScriptedSocket>(std::deque<Step>{response("body", 4)}));
  {
    auto lease = pool->acquire("localhost", 80);
    REQUIRE(lease);
    Reader reader(Direction::Response, std::make_shared<SocketStream>(*lease));
    REQUIRE(reader.readHeaders());
    Response res(std::move(reader));
    auto body = res.text();
    REQUIRE(body);
    CHECK(*body == "body");
  }
  CHECK(pool->acquire("localhost", 80));
}

TEST_CASE("Close-delimited bodies require a clean transport EOF") {
  for (auto error : {std::error_code{}, std::make_error_code(std::errc::connection_reset)}) {
    std::deque<Step> steps{std::string{"HTTP/1.1 200 OK\r\n\r\nabc"}};
    if (error) steps.emplace_back(error);
    Reader reader(Direction::Response, stream(std::move(steps)));
    REQUIRE(reader.readHeaders());
    CHECK_FALSE(reader.contentLengthHint());
    auto body = reader.getBodyStringView();
    if (error) {
      REQUIRE_FALSE(body);
      CHECK(body.error() == error);
    } else {
      REQUIRE(body);
      CHECK(*body == "abc");
    }
  }
}

TEST_CASE("Bodyless responses do not wait for advertised bytes") {
  Reader reader(Direction::Response, stream({response("", 100)}));
  REQUIRE(reader.readHeaders(true));
  auto body = reader.getBodyStringView();
  REQUIRE(body);
  CHECK(body->empty());
  for (int status : {204, 304}) {
    Reader noBody(Direction::Response, stream({"HTTP/1.1 " + std::to_string(status) + " OK\r\n\r\n"}));
    REQUIRE(noBody.readHeaders());
    REQUIRE(noBody.getBodyStringView());
    CHECK(noBody.getBodyStringView()->empty());
  }
}

TEST_CASE("DataStream delivers copied bytes before a pending transport error") {
  auto client = std::make_shared<Client>(std::make_unique<ScriptedTransport>(
      std::deque<Step>{response("abc", 6), std::make_error_code(std::errc::connection_reset)}));
  http::DataStream data(client, 4);
  REQUIRE(data.open(Method::GET, "http://localhost/test", {}));
  std::array<std::byte, 16> out{};
  auto first = data.read(out.data(), out.size());
  REQUIRE(first);
  CHECK(*first == 3);
  auto second = data.read(out.data(), out.size());
  REQUIRE_FALSE(second);
  CHECK(second.error() == std::errc::connection_reset);
}

TEST_CASE("DataStream respects a finite body shorter than its chunk size") {
  auto client = std::make_shared<Client>(std::make_unique<ScriptedTransport>(
      std::deque<Step>{response("abc", 3)}));
  http::DataStream data(client, 4096);
  REQUIRE(data.open(Method::GET, "http://localhost/test", {}));
  std::array<std::byte, 16> out{};
  auto first = data.read(out.data(), out.size());
  REQUIRE(first);
  CHECK(*first == 3);
  auto end = data.read(out.data(), out.size());
  REQUIRE(end);
  CHECK(*end == 0);
}

TEST_CASE("Malformed or unsupported framing fails before a body can be reused") {
  for (const auto& header : {"Content-Length: -1", "Content-Length: 4junk",
                             "Content-Length: 999999999999999999999999999",
                             "Content-Length: 1\r\nContent-Length: 2",
                             "Content-Length: ", "Transfer-Encoding: chunked"}) {
    Reader reader(Direction::Response, stream({std::string{"HTTP/1.1 200 OK\r\n"} + header + "\r\n\r\n"}));
    CHECK_FALSE(reader.readHeaders());
    CHECK_FALSE(reader.getBodyStringView());
  }
}

TEST_CASE("A generic bad stream is an I/O error even when eofbit is also set") {
  std::istringstream input;
  input.setstate(std::ios::badbit | std::ios::eofbit);
  Reader reader(Direction::Request, &input);
  auto res = reader.readHeaders();
  REQUIRE_FALSE(res);
  CHECK(res.error() == std::errc::io_error);
}

TEST_CASE("A complete response stays complete after a later transport failure") {
  auto s = stream({response("abc", 3), std::make_error_code(std::errc::connection_reset)});
  Reader reader(Direction::Response, s);
  REQUIRE(reader.readHeaders());
  REQUIRE(reader.getBodyStringView());
  CHECK(s->get() == std::char_traits<char>::eof());
  CHECK(s->readError() == std::errc::connection_reset);
  auto body = reader.getBodyStringView();
  REQUIRE(body);
  CHECK(*body == "abc");
}

namespace {
class RangeTransport : public Transport {
 public:
  std::deque<Step> responses;
  std::vector<std::string> ranges;
  explicit RangeTransport(std::deque<Step> responses) : responses(std::move(responses)) {}
  Result<Response> execute(const Request& request) override {
    ranges.push_back(request.headers.at("Range"));
    if (responses.empty()) return make_unexpected_errc<Response>(std::errc::io_error);
    Step next = std::move(responses.front());
    responses.pop_front();
    if (auto* error = std::get_if<std::error_code>(&next)) {
      return nonstd::make_unexpected(*error);
    }
    Reader reader(Direction::Response, stream({std::get<std::string>(std::move(next))}));
    auto headers = reader.readHeaders();
    if (!headers) return nonstd::make_unexpected(headers.error());
    return Response(std::move(reader));
  }
};
std::string rangeResponse(const std::string& body, int start) {
  return "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes " +
      std::to_string(start) + "-" + std::to_string(start + 3) +
      "/8\r\nContent-Length: 4\r\n\r\n" + body;
}
}

TEST_CASE("Range reads finish at EOF without requesting an extra range") {
  auto transport = std::make_unique<RangeTransport>(
      std::deque<Step>{rangeResponse("abcd", 0), rangeResponse("efgh", 4)});
  auto* observed = transport.get();
  auto client = std::make_shared<Client>(std::move(transport));
  http::DataStream data(client, 4);
  REQUIRE(data.open(Method::GET, "http://localhost/range", {}));
  std::array<std::byte, 16> buffer{};
  auto read = data.read(buffer.data(), buffer.size());
  REQUIRE(read);
  CHECK(*read == 8);
  CHECK(std::memcmp(buffer.data(), "abcdefgh", 8) == 0);
  auto end = data.read(buffer.data(), buffer.size());
  REQUIRE(end);
  CHECK(*end == 0);
  REQUIRE(observed->ranges.size() == 2);
  CHECK(observed->ranges[0] == "bytes=0-3");
  CHECK(observed->ranges[1] == "bytes=4-7");
}

TEST_CASE("Range reads preserve a truncated body and errors fetching the next range") {
  for (bool truncated : {true, false}) {
    std::deque<Step> responses{rangeResponse("abcd", 0)};
    if (truncated) responses.emplace_back(rangeResponse("ef", 4));
    else responses.emplace_back(std::make_error_code(std::errc::timed_out));
    auto client = std::make_shared<Client>(std::make_unique<RangeTransport>(std::move(responses)));
    http::DataStream data(client, 4);
    REQUIRE(data.open(Method::GET, "http://localhost/range", {}));
    std::array<std::byte, 16> buffer{};
    auto read = data.read(buffer.data(), buffer.size());
    REQUIRE(read);
    CHECK(*read == (truncated ? 6 : 4));
    auto failed = data.read(buffer.data(), buffer.size());
    REQUIRE_FALSE(failed);
    CHECK(failed.error() == (truncated ? make_error_code(Errc::IncompleteMessage) :
                            std::make_error_code(std::errc::timed_out)));
  }
}

TEST_CASE("A pooled connection with an abandoned body is discarded") {
  auto pool = std::make_shared<ConnectionPool>();
  pool->insert("localhost", 80, std::make_unique<ScriptedSocket>(std::deque<Step>{response("body", 4)}));
  {
    auto lease = pool->acquire("localhost", 80);
    REQUIRE(lease);
    Reader reader(Direction::Response, std::make_shared<SocketStream>(*lease));
    REQUIRE(reader.readHeaders());
  }
  CHECK_FALSE(pool->acquire("localhost", 80));
}

TEST_CASE("A transport error before any body bytes remains specific") {
  for (auto error : {std::errc::timed_out, std::errc::connection_reset}) {
    Reader reader(Direction::Response, stream({response("", 4), std::make_error_code(error)}));
    REQUIRE(reader.readHeaders());
    auto body = reader.getBodyStringView();
    REQUIRE_FALSE(body);
    CHECK(body.error() == error);
  }
}
