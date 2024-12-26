#pragma once

#include <array>
#include <vector>

#include "bell/net/SocketStream.h"
#include "bell/net/TCPSocket.h"
#include "bell/utils/Task.h"

namespace bell::net {
class HTTPServer : bell::utils::Task {
 public:
  HTTPServer(int maxConnections = 5);

  void listen(int port = 8080);

 private:
  const char* LOG_TAG = "HTTPServer";

  struct ClientConnection {
    static constexpr size_t bufferSize = 1024;

    std::unique_ptr<bell::net::SocketStream> socket;
    std::array<uint8_t, bufferSize> requestBuffer{};
  };

  int maxConnections;

  std::unique_ptr<bell::net::TCPSocket> listenSocket;

  void acceptConnection();

  void readFromClient(const ClientConnection& connection);

  void taskLoop() override;

  // ::select() related members
  int maxFd = 0;
  fd_set masterFdSet{};

  std::vector<ClientConnection> clientSockets{};
};
}  // namespace bell::net
