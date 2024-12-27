#include "bell/net/HTTPServer.h"

#include "bell/Logger.h"
#include "bell/utils/Task.h"
#include "bell/utils/Utils.h"
#include "picohttpparser.h"

#include <sys/select.h>
#include <iostream>

using namespace bell;

net::HTTPServer::HTTPServer(int maxConnections)
    : utils::Task("bell::net::HTTPServer", 1024),
      maxConnections(maxConnections) {}

void net::HTTPServer::listen(int port) {
  // Stop the task if it's already running
  stopTask();

  listenSocket = std::make_unique<net::TCPSocket>();

  // Try to bind to the specified port
  listenSocket->bind("localhost", port);

  // Start listening for incoming connections
  listenSocket->listen(maxConnections);

  // Prepare master fd set for select
  FD_ZERO(&masterFdSet);
  FD_SET(listenSocket->getFd(), &masterFdSet);
  maxFd = listenSocket->getFd();

  startTask();  // Will begin the task loop
}

// void net::HTTPServer::acceptConnection() {
//   // Accept the connection
//   auto clientSocket = listenSocket->accept();
//   int clientFd = clientSocket->getFd();
//   FD_SET(clientFd, &masterFdSet);
//   if (clientFd > maxFd) {
//     maxFd = clientFd;
//   }
//   clientSockets.push_back(
//       {std::make_unique<SocketStream>(std::move(clientSocket))});
// }

// void net::HTTPServer::readFromClient(const ClientConnection& connection) {
//   const char* methodPtr = nullptr;
//   size_t methodLen = 0;

//   const char* pathPtr = nullptr;
//   size_t pathLen = 0;

//   int minorVersion = 0;

//   std::string line;  // Use this to read lines from the stream
//   size_t numHeaders = 0;

//   std::vector<char> responseBuffer;
//   std::vector<phr_header> phResponseHeaders(2);

//   while (std::getline(*connection.socket, line) && !line.empty()) {
//     // Restore the newline character
//     line.append("\n");

//     responseBuffer.insert(responseBuffer.end(), line.begin(), line.end());

//     // Reserve space for the headers
//     phResponseHeaders.push_back({});
//     numHeaders = phResponseHeaders.size();

//     int ret = phr_parse_request(
//         responseBuffer.data(), responseBuffer.size(), &methodPtr, &methodLen,
//         &pathPtr, &pathLen, &minorVersion, phResponseHeaders.data(),
//         &numHeaders, responseBuffer.size() - line.size());

//     if (ret > 0) {
//       break;  // Successfully parsed the headers, done reading
//     }

//     if (ret == -1) {
//       throw std::runtime_error("Failed to parse HTTP response headers");
//     }
//   }

//   std::cout << "Successfully read request" << std::endl;
//   std::cout << "Method: " << std::string(methodPtr, methodLen) << std::endl;
//   std::cout << "Path: " << std::string(pathPtr, pathLen) << std::endl;
//   std::cout << "Minor version: " << minorVersion << std::endl;

//   for (size_t i = 0; i < numHeaders; i++) {
//     std::cout << "Header: "
//               << std::string(phResponseHeaders[i].name,
//                              phResponseHeaders[i].name_len)
//               << ": "
//               << std::string(phResponseHeaders[i].value,
//                              phResponseHeaders[i].value_len)
//               << std::endl;
//   }
// }

void net::HTTPServer::taskLoop() {
  fd_set readFdSet = masterFdSet;

  auto selectTV = bell::utils::millisecondsToTimeval(1000);

  // Wait for activity on the sockets
  if (::select(maxFd + 1, &readFdSet, nullptr, nullptr, &selectTV) < 0) {
    BELL_LOG(error, LOG_TAG, "Error in select errno={}, closing the server",
             strerror(errno));
    taskRunning = false;
    return;
  }

  // Check for new connections
  if (FD_ISSET(listenSocket->getFd(), &readFdSet)) {
    // acceptConnection();
  }

  // // Handle data from each connected client
  // for (auto it = clientSockets.begin(); it != clientSockets.end();) {
  //   int clientFd = (*it).socket->getFd();
  //   if (FD_ISSET(clientFd, &readFdSet)) {
  //     readFromClient(*it);
  //   }
  //   ++it;
  // }
}
