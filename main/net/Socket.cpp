#include "bell/net/Socket.h"

#include <system_error>

using namespace bell::net;

bell::Result<size_t> Socket::writeAll(const std::byte* buf, size_t len) {
  size_t written = 0;

  while (written < len) {
    auto res = write(buf + written, len - written);
    if (!res) {
      if (res.error() == std::errc::interrupted) {
        continue;
      }
      // On an already-blocking socket (see this method's own doc comment),
      // would-block can only mean its send timeout fired - surfaced as
      // timed_out rather than the raw would-block errc, matching how a read
      // timeout is reported elsewhere in bell (SocketStream.cpp).
      if (res.error() == std::errc::operation_would_block) {
        return bell::make_unexpected_errc<size_t>(std::errc::timed_out);
      }
      return res;
    }

    // A successful write() call reporting 0 bytes sent for a non-empty
    // buffer isn't expected to make forward progress on retry either -
    // surfaced as a real error instead of looping forever.
    if (*res == 0) {
      return bell::make_unexpected_errc<size_t>(std::errc::io_error);
    }

    written += *res;
  }

  return written;
}
