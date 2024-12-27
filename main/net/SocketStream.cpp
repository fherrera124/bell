#include "bell/net/SocketStream.h"

#include "bell/Logger.h"

#include <cstdint>  // for uint8_t
#include <cstdio>   // for NULL, ssize_t
#include <memory>

using namespace bell::net;

int SocketBuffer::open(std::unique_ptr<Socket> socket, int operationTimeoutMs) {
  if (internalSocket != nullptr) {
    close();
  }
  this->operationTimeoutMs = operationTimeoutMs;
  internalSocket = std::move(socket);

  // Set the socket to non-blocking mode if a timeout is specified
  internalSocket->setBlocking(operationTimeoutMs == 0);

  return 0;
}

int SocketBuffer::close() {
  if (internalSocket != nullptr && isOpen()) {
    pubsync();
    internalSocket->close();
    internalSocket = nullptr;
  }
  return 0;
}

int SocketBuffer::sync() {
  size_t n = pptr() - pbase();
  try {
    while (n > 0) {
      auto bw = internalSocket->write(reinterpret_cast<uint8_t*>(pptr() - n), n,
                                      operationTimeoutMs);
      n -= bw;
    }
  } catch (const std::exception& e) {
    BELL_LOG(error, "SocketBuffer", "Error writing to socket: {}", e.what());
    setp(pptr() - n, obuf.data() + bufLen);
    pbump(n);
    return -1;
  }
  setp(obuf.data(), obuf.data() + bufLen);
  return 0;
}

SocketBuffer::int_type SocketBuffer::underflow() {
  size_t br = 0;
  try {
    br = internalSocket->read(reinterpret_cast<uint8_t*>(ibuf.data()), bufLen,
                              operationTimeoutMs);
  } catch (std::exception& e) {
    setg(nullptr, nullptr, nullptr);
    return traits_type::eof();
  }
  setg(ibuf.data(), ibuf.data(), ibuf.data() + br);
  return traits_type::to_int_type(*ibuf.data());
}

SocketBuffer::int_type SocketBuffer::overflow(int_type c) {
  if (sync() < 0)
    return traits_type::eof();
  if (traits_type::eq_int_type(c, traits_type::eof()))
    return traits_type::not_eof(c);
  *pptr() = traits_type::to_char_type(c);
  pbump(1);
  return c;
}

std::streamsize SocketBuffer::xsgetn(char_type* _s, std::streamsize _n) {
  const std::streamsize bn = egptr() - gptr();
  if (_n <= bn) {
    traits_type::copy(_s, gptr(), _n);
    gbump(_n);
    return _n;
  }
  traits_type::copy(_s, gptr(), bn);
  setg(nullptr, nullptr, nullptr);
  std::streamsize remain = _n - bn;
  char_type* end = _s + _n;
  size_t br;
  try {
    while (remain > 0) {
      br = internalSocket->read(reinterpret_cast<uint8_t*>(end - remain),
                                remain, operationTimeoutMs);
      if (br == 0) {
        return (_n - remain);
      }
      remain -= br;
    }
  } catch (...) { return (_n - remain); }
  return _n;
}

std::streamsize SocketBuffer::xsputn(const char_type* _s, std::streamsize _n) {
  if (pptr() + _n <= epptr()) {
    traits_type::copy(pptr(), _s, _n);
    pbump(_n);
    return _n;
  }
  if (sync() < 0)
    return 0;
  ssize_t bw;
  std::streamsize remain = _n;
  const char_type* end = _s + _n;
  try {
    while (remain > bufLen) {
      bw = internalSocket->write((uint8_t*)(end - remain), remain,
                                 operationTimeoutMs);
      remain -= bw;
    }
  } catch (...) { return (_n - remain); }
  if (remain > 0) {
    traits_type::copy(pptr(), end - remain, remain);
    pbump(remain);
  }
  return _n;
}
