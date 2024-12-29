#include "bell/http/Server.h"

#include "bell/Logger.h"
#include "bell/http/Common.h"
#include "bell/http/Reader.h"
#include "bell/net/SocketStream.h"
#include "bell/net/TCPSocket.h"
#include "bell/utils/Task.h"
#include "bell/utils/Utils.h"

#include <sys/select.h>
#include <unistd.h>
#include <iostream>
#include <regex>
#include <stdexcept>

using namespace bell;

http::Server::Server(int maxConnections)
    : utils::Task("bell::net::HTTPServer", 16 * 1024),
      maxConnections(maxConnections) {
  notFoundHandler = [](const auto& /*requestReader*/,
                       const auto& responseWriter, const auto& /*params*/) {
    responseWriter->writeResponseWithBody(404, {}, "Not found");
  };
}

void http::Server::listen(int port) {
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
  BELL_LOG(info, LOG_TAG, "Server listening on port {}", port);
}

void http::Server::registerCustom404(const RequestHandler& handler) {
  notFoundHandler = handler;
}

void http::Server::acceptConnection() {
  // Accept the connection
  std::shared_ptr<net::TCPSocket> clientSocket = listenSocket->accept();
  int clientFd = clientSocket->getFd();
  FD_SET(clientFd, &masterFdSet);
  BELL_LOG(debug, LOG_TAG, "Accepted connection");
  connections.push_back({std::move(clientSocket), false});
}

void http::Server::registerHandler(Method method, const std::string& path,
                                   const RequestHandler& handler) {
  router.insert(method, path, handler);
}

void http::Server::registerGet(const std::string& path,
                               const RequestHandler& handler) {
  registerHandler(Method::GET, path, handler);
}

void http::Server::registerPost(const std::string& path,
                                const RequestHandler& handler) {
  registerHandler(Method::POST, path, handler);
}

void http::Server::closeConnection(int fd) {
  for (auto it = connections.begin(); it != connections.end();) {
    if (it->socket->getFd() == fd) {
      // Mark the connection as closed
      it->closed = true;
      return;
    }
  }
}

void http::Server::readFromClient(const Connection& connection) {
  try {
    // Wrap the socket in a stream, try to parse the request
    net::SocketStream socketStream(connection.socket,
                                   defaultHttpOperationTimeout);

    auto reader = std::make_unique<http::Reader>(Direction::Request,
                                                 &socketStream, &readBuffer);
    reader->readHeaders();

    auto writer =
        std::make_unique<http::Writer>(Direction::Response, &socketStream);
    writer->setHeader("Connection", "close");

    // Find the handler for the request
    auto handler = router.find(reader->getMethod(), reader->getPath());

    if (!handler) {
      notFoundHandler(reader, writer, {});
    } else {
      try {
        handler->first(reader, writer, handler->second);
      } catch (const std::exception& e) {
        BELL_LOG(error, LOG_TAG, "Error occured in the request handler: {}",
                 e.what());
        writer->writeResponseWithBody(500, {}, "Internal server error");
      }
    }

    if (!writer->hasWrittenHeaders() || !writer->hasWrittenBody()) {
      BELL_LOG(error, LOG_TAG, "Handler did not write response");
    }

    closeConnection(connection.socket->getFd());
  } catch (const std::exception& e) {
    BELL_LOG(error, LOG_TAG, "Error occured while processing request: {}",
             e.what());
    closeConnection(connection.socket->getFd());
  }
}

void http::Server::taskLoop() {
  fd_set readFdSet = masterFdSet;

  auto selectTV = bell::utils::millisecondsToTimeval(1000);

  maxFd = listenSocket->getFd();
  for (const auto& it : connections) {
    if (it.socket->getFd() > maxFd) {
      maxFd = it.socket->getFd();
    }
  }

  // Wait for activity on the sockets
  if (::select(maxFd + 1, &readFdSet, nullptr, nullptr, &selectTV) < 0) {
    BELL_LOG(error, LOG_TAG, "Error in select errno={}, closing the server",
             strerror(errno));
    taskRunning = false;
    return;
  }

  // Check for new connections
  if (FD_ISSET(listenSocket->getFd(), &readFdSet)) {
    acceptConnection();
  }

  // Handle data from each connected client
  for (auto it = connections.begin(); it != connections.end();) {
    int clientFd = it->socket->getFd();
    if (FD_ISSET(clientFd, &readFdSet)) {
      readFromClient(*it);
    }

    if (it->closed) {
      BELL_LOG(debug, LOG_TAG, "Closing connection");
      it->socket->close();
      FD_CLR(it->socket->getFd(), &masterFdSet);
      it = connections.erase(it);
    } else {
      ++it;
    }
  }
}
