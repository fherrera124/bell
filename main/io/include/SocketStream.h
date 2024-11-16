#pragma once

#include <array>
#include <iostream>  // for streamsize, basic_streambuf<>::int_type, ios...
#include <memory>    // for unique_ptr, operator!=
#include <string>    // for char_traits, string

#include "Socket.h"  // for Socket

namespace bell::io {
class SocketBuffer : public std::streambuf {
 private:
  std::unique_ptr<Socket> internalSocket;

  // Timeout for socket operations in milliseconds, 0 means that the socket is blocking
  int operationTimeoutMs;

  static const int bufLen = 1024;
  std::array<char, bufLen> ibuf{};
  std::array<char, bufLen> obuf{};

 public:
  SocketBuffer() = default;

  // Delete copy constructor and copy assignment operator
  SocketBuffer(const SocketBuffer&) = delete;
  SocketBuffer& operator=(const SocketBuffer&) = delete;

  // Define move constructor and move assignment operator
  SocketBuffer(SocketBuffer&& other) noexcept = default;
  SocketBuffer& operator=(SocketBuffer&& other) noexcept = default;

  int open(std::unique_ptr<Socket> socket, int operationTimeoutMs = 0);

  int close();

  bool isOpen() {
    return internalSocket != nullptr && internalSocket->isOpen();
  }

  ~SocketBuffer() { close(); }

 protected:
  int sync() override;

  int_type underflow() override;

  int_type overflow(int_type c = traits_type::eof()) override;

  std::streamsize xsgetn(char_type* _s, std::streamsize _n) override;

  std::streamsize xsputn(const char_type* _s, std::streamsize _n) override;
};

class SocketStream : public std::iostream {
 private:
  SocketBuffer socketBuf;

 public:
  SocketStream() : std::iostream(&socketBuf) {}

  SocketStream(std::unique_ptr<Socket> socket, int operationTimeoutMs = 0)
      : std::iostream(&socketBuf) {
    open(std::move(socket), operationTimeoutMs);
  }

  SocketBuffer* rdbuf() { return &socketBuf; }

  int open(std::unique_ptr<Socket> socket, int operationTimeoutMs = 0) {
    int err = socketBuf.open(std::move(socket), operationTimeoutMs);
    if (err)
      setstate(std::ios::failbit);
    return err;
  }

  int close() { return socketBuf.close(); }

  bool isOpen() { return socketBuf.isOpen(); }
};
}  // namespace bell::io
