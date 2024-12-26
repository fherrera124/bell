#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "bell/Logger.h"
#include "bell/net/SocketPollListener.h"
#include "bell/net/TCPSocket.h"
#include "bell/utils/Utils.h"

namespace {
std::atomic<bool> echoServerRunning = false;
std::mutex echoServerMutex;
void runEchoServer(bell::net::TCPSocket* serverSocket) {
  std::scoped_lock lock(echoServerMutex);
  echoServerRunning = true;

  bell::net::SocketPollListener pollListener;

  std::vector<std::unique_ptr<bell::net::TCPSocket>> clientSockets;

  pollListener.registerSocket(
      serverSocket->getFd(), POLLRDNORM,
      [&pollListener, serverSocket, &clientSockets](short /*event*/) {
        BELL_LOG(info, "Echo server", "Accepting new connection");
        // Handle ::accept
        clientSockets.emplace_back(serverSocket->accept());

        bell::net::TCPSocket* clientSocket = clientSockets.back().get();

        pollListener.registerSocket(
            clientSocket->getFd(), POLLIN | POLLHUP,
            [clientSocket](short event) {
              if (event & POLLHUP) {
                BELL_LOG(info, "Echo server", "Client disconnected");
                return;
              }
              // Handle client socket read
              std::string buffer;
              buffer.resize(1024);
              size_t bytesRead = clientSocket->read(
                  reinterpret_cast<uint8_t*>(buffer.data()), buffer.size());
              if (bytesRead > 0) {
                BELL_LOG(info, "Echo server", "Received {} bytes", bytesRead);
                clientSocket->write(
                    reinterpret_cast<const uint8_t*>(buffer.data()), bytesRead);
              }
            });
      });
  while (echoServerRunning) {
    pollListener.poll(100);
  }

  // Stop the server
  serverSocket->close();
}
}  // namespace

TEST_CASE("bell::io::Socket and derieved classes tests", "[bell::io::Socket]") {
  int echoServerPort = 7542;
  auto echoServerSocket = std::make_unique<bell::net::TCPSocket>();

  // Bind the socket to the echo server port
  REQUIRE_NOTHROW(echoServerSocket->bind("127.0.0.1", echoServerPort));

  // Bind should have opened the socket
  REQUIRE(echoServerSocket->isOpen());

  // Listen on the socket, with a backlog of 5
  REQUIRE_NOTHROW(echoServerSocket->listen(5));

  // Start the echo server runner
  std::thread echoServerRunner(runEchoServer, echoServerSocket.get());

  // Ensure the server is running
  bell::utils::sleepMs(500);

  // Test the TCP client for basic operations
  auto clientSocket = std::make_unique<bell::net::TCPSocket>();

  SECTION("Connect to echo server") {
    REQUIRE_NOTHROW(clientSocket->connect("127.0.0.1", echoServerPort));
    REQUIRE(clientSocket->isOpen());
  }

  SECTION("Write to and read from echo server") {
    REQUIRE_NOTHROW(clientSocket->connect("127.0.0.1", echoServerPort));
    std::string message = "Hello, Echo Server!";
    size_t bytesWritten = clientSocket->write(
        reinterpret_cast<const uint8_t*>(message.data()), message.size());
    REQUIRE(bytesWritten == message.size());

    std::string readBuffer(1024, '\0');
    size_t bytesRead = clientSocket->read(
        reinterpret_cast<uint8_t*>(readBuffer.data()), readBuffer.size());

    REQUIRE(bytesRead == message.size());
    REQUIRE(readBuffer.substr(0, bytesRead) == message);
  }

  SECTION("Connect timeout handling") {
    // Test if the client handles connection timeouts correctly
    clientSocket->setBlocking(false);
    REQUIRE_THROWS(clientSocket->connect("10.255.255.1", echoServerPort,
                                         500));  // Impossible address for demo
  }

  SECTION("Other basic operations") {
    // Example of closing the socket
    REQUIRE_NOTHROW(clientSocket->connect("127.0.0.1", echoServerPort));
    REQUIRE(clientSocket->isOpen());
    clientSocket->close();
    REQUIRE_FALSE(clientSocket->isOpen());
  }

  {
    // Stop the echo server
    echoServerRunning = false;
    std::scoped_lock lock(echoServerMutex);
    echoServerRunner.join();
  }
}
