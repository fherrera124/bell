# HTTP read termination tests

From the Bell repository, with submodules initialized:

```sh
cmake -S test/read-termination -B build/read-termination -DBELL_DISABLE_SANITIZERS=OFF
cmake --build build/read-termination --parallel 4
ctest --test-dir build/read-termination --output-on-failure
```

This standalone host suite requires doctest, Python 3 and the OpenSSL command-line
program. It does not build firmware or require internet access. Set
`DOCTEST_INCLUDE_DIR` to an existing directory containing `doctest/doctest.h` if
that submodule is unavailable. `BELL_DISABLE_MBEDTLS=ON` excludes the TLS cases.

Coverage includes fragmented and consecutive requests, orderly EOF, partial
headers and bodies, timeout/reset propagation, deferred errors after partial
reads, connection pool disposal, Reader moves, and DataStream reads. Local socket
tests check real timeout behavior and server logging. TLS fixtures use an ephemeral
self-signed certificate and exercise both `close_notify` and abrupt TCP closure,
including closure after a complete Content-Length response. Sanitizers instrument
Bell and the test executables.

## Read contract

- `SocketStream::readState()` distinguishes ready, orderly EOF and failure;
  `readError()` preserves the transport error. Clearing iostream flags does not
  reset a terminal socket read state.
- `Reader::readHeaders()` returns `http::Errc::EndOfStream` only for orderly EOF
  before any header bytes. A server awaiting another request treats this as a
  normal close; a client awaiting a response still fails the request.
- Partial headers or a body shorter than Content-Length return
  `http::Errc::IncompleteMessage`, unless a specific transport error explains the
  failure. TLS closure without `close_notify` retains the MbedTLS EOF error.
- `Response::readBodyChunk()` delivers any received bytes first and reports a
  pending failure on the next call. Zero means the body is complete (or the
  requested read length was zero). Whole-body accessors fail on truncation.
- Responses without Content-Length are read until orderly connection closure and
  cannot be pooled. `Response::contentLength` is empty for those responses.
  Transfer-Encoding decoding is unsupported and returns `not_supported`.
- `Response::stream()` remains available, but raw reads bypass HTTP body
  validation. Callers must validate byte counts and errors themselves.

The ESP32-S2 compiler reports a 12-byte increase for SocketStream and a 4-byte
increase for Reader. DataStream grows by 8 bytes in total, including its embedded
Reader. These are object sizes, not measurements of runtime free heap. Read state
adds no dynamic allocation or task; received bodies use the existing body buffer.
