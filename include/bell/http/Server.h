#pragma once

#include <array>
#include <vector>

#include "bell/net/SocketStream.h"
#include "bell/net/TCPSocket.h"
#include "bell/utils/Task.h"

namespace bell::http {
class Server : bell::utils::Task {
 public:
  Server(int maxConnections = 5);

  void listen(int port = 8080);

 private:
  const char* LOG_TAG = "HTTPServer";

  int maxConnections;

  std::unique_ptr<bell::net::TCPSocket> listenSocket;

  struct Connection {
    std::unique_ptr<bell::net::SocketStream> socket;
    int fd;
  };

  void acceptConnection();

  void readFromClient(const Connection& connection);

  void taskLoop() override;

  // ::select() related members
  int maxFd = 0;
  fd_set masterFdSet{};

  const int maxReadBufferSize = 16 * 1024;
  std::vector<char> readBuffer{};

  // std::vector<Connection> connections{};
};
}  // namespace bell::net
