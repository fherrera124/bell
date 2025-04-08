#include "bell/net/SocketStream.h"

#include "bell/Logger.h"

#include <cstdint>  // for uint8_t
#include <cstdio>   // for NULL, ssize_t
#include <memory>

using namespace bell::net;

SocketBuffer::SocketBuffer(std::shared_ptr<Socket> socket)
    : internalSocket(std::move(socket)) {}

int SocketBuffer::sync() {
  size_t n = pptr() - pbase();
  while (n > 0) {
    auto bw = internalSocket->write(reinterpret_cast<uint8_t*>(pptr() - n), n);

    if (!bw) {
      BELL_LOG(error, "SocketBuffer", "Error writing to socket: {}",
               bw.errorMessage());
      setp(pptr() - n, obuf.data() + bufLen);
      pbump(n);
      return -1;
    }
    n -= bw.getValue();
  }

  setp(obuf.data(), obuf.data() + bufLen);
  return 0;
}

SocketBuffer::int_type SocketBuffer::underflow() {
  auto br =
      internalSocket->read(reinterpret_cast<uint8_t*>(ibuf.data()), bufLen);
  if (!br) {
    setg(nullptr, nullptr, nullptr);
    return traits_type::eof();
  }
  setg(ibuf.data(), ibuf.data(), ibuf.data() + br.getValue());
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
  while (remain > 0) {
    auto br =
        internalSocket->read(reinterpret_cast<uint8_t*>(end - remain), remain);

    if (!br) {
      return (_n - remain);
    }

    if (br.getValue() == 0) {
      return (_n - remain);
    }
    remain -= br.getValue();
  }
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
  std::streamsize remain = _n;
  const char_type* end = _s + _n;
  while (remain > bufLen) {
    auto bw = internalSocket->write((uint8_t*)(end - remain), remain);

    if (!bw) {
      return (_n - remain);
    }
    remain -= bw.getValue();
  }

  if (remain > 0) {
    traits_type::copy(pptr(), end - remain, remain);
    pbump(remain);
  }
  return _n;
}
