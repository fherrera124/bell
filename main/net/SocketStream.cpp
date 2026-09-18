#include "bell/net/SocketStream.h"

#include "bell/Logger.h"

#include <cstdint>  // for uint8_t
#include <cstdio>   // for NULL, ssize_t
#include <functional>
#include <memory>

using namespace bell::net;

namespace {
// SocketBuffer forces the socket into blocking mode (see the constructor
// below), so read()/write() themselves block on the kernel/lwIP until data
// or room is available, or until the per-request timeout set via
// setReceiveTimeout()/setSendTimeout() (DefaultTransport::execute()) fires -
// no application-level retry loop needed to wait for readiness. POSIX
// defines SO_RCVTIMEO/SO_SNDTIMEO expiry as reporting EAGAIN/EWOULDBLOCK,
// the same errno a non-blocking socket reports for "nothing ready right
// now" - on a blocking socket that can only mean the timeout fired, so it's
// surfaced as a real timeout rather than retried. EINTR is retried
// immediately since it's not a real failure, just an interrupted syscall.
bell::Result<size_t> retryOnEintr(
    const std::function<bell::Result<size_t>()>& op) {
  while (true) {
    auto res = op();
    if (res) {
      return res;
    }
    if (res.error() == std::errc::interrupted) {
      continue;
    }
    if (res.error() == std::errc::operation_would_block) {
      return bell::make_unexpected_errc<size_t>(std::errc::timed_out);
    }
    return res;
  }
}
}  // namespace

SocketBuffer::SocketBuffer(std::shared_ptr<Socket> socket)
    : internalSocket(std::move(socket)) {
  setg(ibuf.data(), ibuf.data(), ibuf.data());
  setp(obuf.data(), obuf.data() + bufLen);
  // See retryOnEintr() above - blocking-with-timeout is what every call site
  // here expects, regardless of what mode connect() left the socket in.
  (void)internalSocket->setBlocking(true);
}

int SocketBuffer::sync() {
  size_t n = pptr() - pbase();
  // Reset the put area either way: writeAll() doesn't report how many bytes
  // it got out before a failure, and there's no way to know how many of the
  // n bytes buffered here still need resending - not a real gap in
  // practice, since every caller (DefaultTransport::execute()) closes and
  // discards the connection on a write failure rather than retrying this
  // same SocketBuffer.
  setp(obuf.data(), obuf.data() + bufLen);
  if (n > 0) {
    auto bw = internalSocket->writeAll(
        reinterpret_cast<std::byte*>(pbase()), n);
    if (!bw) {
      BELL_LOG(error, "SocketBuffer", "Write failed: {}", bw.error());
      return -1;  // This will make the stream set failbit
    }
  }
  return 0;
}

SocketBuffer::int_type SocketBuffer::underflow() {
  if (gptr() < egptr()) {
    return traits_type::to_int_type(*gptr());
  }
  auto br = readSocket(reinterpret_cast<std::byte*>(ibuf.data()), bufLen);
  if (!br || *br == 0) {
    return traits_type::eof();
  }
  setg(ibuf.data(), ibuf.data(), ibuf.data() + *br);
  return traits_type::to_int_type(*ibuf.data());
}

bell::Result<size_t> SocketBuffer::readSocket(std::byte* dst, size_t len) {
  if (readState_ == ReadState::Error) {
    return nonstd::make_unexpected(readError_);
  }
  if (readState_ == ReadState::EndOfStream) {
    return size_t{0};
  }
  auto res = retryOnEintr([&] { return internalSocket->read(dst, len); });
  if (!res) {
    readError_ = res.error();
    readState_ = ReadState::Error;
  } else if (*res == 0) {
    readState_ = ReadState::EndOfStream;
  } else {
    bytesRead_ += *res;
  }
  return res;
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
  if (_n <= 0) {
    return 0;
  }
  const std::streamsize bn = egptr() - gptr();
  if (_n <= bn) {
    traits_type::copy(_s, gptr(), _n);
    gbump(_n);
    return _n;
  }
  traits_type::copy(_s, gptr(), bn);
  setg(ibuf.data(), ibuf.data(), ibuf.data());
  std::streamsize remain = _n - bn;
  char_type* end = _s + _n;
  while (remain > 0) {
    auto br = readSocket(reinterpret_cast<std::byte*>(end - remain), remain);

    if (!br) {
      return (_n - remain);
    }

    if (*br == 0) {
      return (_n - remain);
    }
    remain -= *br;
  }
  return _n;
}

std::streamsize SocketBuffer::xsputn(const char_type* s, std::streamsize n) {
  if (n <= 0) {
    return 0;
  }
  if (n <= epptr() - pptr()) {
    traits_type::copy(pptr(), s, n);
    pbump(n);
    return n;
  }
  if (sync() < 0) {
    return 0;  // Stream sets failbit
  }

  // Anything beyond one bufLen-sized chunk goes straight to the socket
  // (writeAll() loops internally); whatever's left (always <= bufLen, since
  // sync() above just emptied obuf) gets buffered normally below.
  std::streamsize bulk = n > bufLen ? n - bufLen : 0;
  if (bulk > 0) {
    auto bw = internalSocket->writeAll(reinterpret_cast<const std::byte*>(s),
                                       bulk);
    if (!bw) {
      return 0;  // Stream sets failbit
    }
  }

  std::streamsize remain = n - bulk;
  if (remain > 0) {
    traits_type::copy(pptr(), s + bulk, remain);
    pbump(remain);
  }
  return n;
}
