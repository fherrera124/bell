#pragma once

#include <array>
#include <iostream>  // for streamsize, basic_streambuf<>::int_type, ios...
#include <memory>    // for unique_ptr, operator!=
#include <string>    // for char_traits, string

#include "bell/net/Socket.h"  // for Socket

namespace bell::net {
enum class ReadState : uint8_t { Ready, EndOfStream, Error };

class SocketBuffer : public std::streambuf {
 private:
  std::shared_ptr<Socket> internalSocket = nullptr;

  static const int bufLen = 256;
  std::array<char, bufLen> ibuf{};
  std::array<char, bufLen> obuf{};

  // Bytes read() from the socket so far. Only touched in underflow()/xsgetn().
  size_t bytesRead_ = 0;
  ReadState readState_ = ReadState::Ready;
  std::error_code readError_;

  bell::Result<size_t> readSocket(std::byte* dst, size_t len);

 public:
  SocketBuffer(std::shared_ptr<Socket> socket);

  // Delete copy constructor and copy assignment operator
  SocketBuffer(const SocketBuffer&) = delete;
  SocketBuffer& operator=(const SocketBuffer&) = delete;

  // Define move constructor and move assignment operator
  SocketBuffer(SocketBuffer&& other) noexcept = default;
  SocketBuffer& operator=(SocketBuffer&& other) noexcept = default;

  // Bytes handed to callers so far (bytesRead_ minus what's still buffered).
  size_t totalBytesConsumed() const {
    return bytesRead_ - static_cast<size_t>(egptr() - gptr());
  }

  // Terminal read state persists even if the owning iostream is cleared.
  ReadState readState() const { return readState_; }
  std::error_code readError() const { return readError_; }

 protected:
  int sync() override;

  int_type underflow() override;

  int_type overflow(int_type c = traits_type::eof()) override;

  std::streamsize xsgetn(char_type* _s, std::streamsize _n) override;

  std::streamsize xsputn(const char_type* _s, std::streamsize _n) override;
};

class SocketStream : public std::iostream {
 protected:
  SocketBuffer socketBuf;
  std::shared_ptr<Socket> socket;

 public:
  SocketStream(const std::shared_ptr<Socket>& socket)
      : std::iostream(&socketBuf), socketBuf(socket), socket(socket) {}

  bool isOpen() { return socket->isValid(); }

  SocketBuffer* rdbuf() { return &socketBuf; }

  size_t totalBytesConsumed() const { return socketBuf.totalBytesConsumed(); }

  ReadState readState() const { return socketBuf.readState(); }
  std::error_code readError() const { return socketBuf.readError(); }

  void close() { socket->close(); }
};
}  // namespace bell::net

namespace bell {
using SocketStream = net::SocketStream;
}
