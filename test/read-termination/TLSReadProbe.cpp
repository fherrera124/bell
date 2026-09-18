#include <iostream>
#include <string>

#include "bell/http/Reader.h"
#include "bell/net/TLSSocket.h"

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  auto socket = std::make_shared<bell::net::TLSSocket>();
  auto connected = socket->connect("127.0.0.1", std::stoi(argv[1]), 2000);
  if (!connected) {
    std::cerr << "connect: " << connected.error().message() << '\n';
    return 1;
  }
  auto stream = std::make_shared<bell::net::SocketStream>(socket);
  bell::http::Reader reader(bell::http::Direction::Response, stream);
  auto headers = reader.readHeaders();
  std::error_code error;
  if (!headers) {
    error = headers.error();
  } else {
    auto body = reader.getBodyStringView();
    if (!body) error = body.error();
    else if (*body != "abc") {
      std::cerr << "unexpected body\n";
      return 1;
    }
  }
  const std::string expected = argv[2];
  bool matched = expected == "success" && !error;
  matched |= expected == "eof" && error == bell::http::Errc::EndOfStream;
  matched |= expected == "incomplete" && error == bell::http::Errc::IncompleteMessage;
  matched |= expected == "tls" && error == bell::net::make_tls_error_code(MBEDTLS_ERR_SSL_CONN_EOF);
  if (!matched) {
    std::cerr << "expected " << expected << ", got " << error.category().name()
              << ':' << error.value() << ' ' << error.message() << '\n';
    return 1;
  }
  return 0;
}
