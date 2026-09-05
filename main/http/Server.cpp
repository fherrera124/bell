#include "bell/http/Server.h"

#include "bell/Logger.h"
#include "bell/http/Common.h"
#include "bell/http/Reader.h"
#include "bell/net/SocketStream.h"
#include "bell/net/TCPSocket.h"
#include "bell/utils/Task.h"
#include "bell/utils/Utils.h"
#include "nonstd/expected.hpp"

#include <sys/select.h>
#include <unistd.h>

using namespace bell;

http::Server::Server(int maxConnections)
    // espStackOnPsram=false: no particular need for PSRAM here, and
    // not every board has it.
    : utils::Task("bell::net::HTTPServer", 16 * 1024, /*espPriority=*/0,
                  TaskCore::CoreAny, /*espStackOnPsram=*/false),
      maxConnections(maxConnections) {
  notFoundHandler = [](const auto& /*requestReader*/,
                       const auto& responseWriter, const auto& /*params*/) {
    (void)responseWriter->writeResponseWithBody(404, {}, "Not found");
  };
}

http::Server::~Server() {
  stopTask();
  if (listenSocket.isValid()) {
    listenSocket.close();
  }
}

bell::Result<> http::Server::listen(int port) {
  // Stop the task if it's already running
  stopTask();

  if (listenSocket.isValid()) {
    listenSocket.close();
  }

  // Try to bind to the specified port
  auto listenRes = listenSocket.bind("", port);
  if (!listenRes) {
    return nonstd::make_unexpected(listenRes.error());
  }

  // Set the socket to non-blocking mode
  auto res = listenSocket.setBlocking(false);
  if (!res) {
    return nonstd::make_unexpected(res.error());
  }

  // Start listening for incoming connections
  res = listenSocket.listen(maxConnections);
  if (!res) {
    return nonstd::make_unexpected(res.error());
  }

  // Prepare master fd set for select
  FD_ZERO(&masterFdSet);
  FD_SET(listenSocket.getFd(), &masterFdSet);
  maxFd = listenSocket.getFd();

  startTask();  // Will begin the task loop
  BELL_LOG(info, LOG_TAG, "Server listening on port {}", *listenRes);

  return {};
}

void http::Server::registerCustom404(const RequestHandler& handler) {
  notFoundHandler = handler;
}

void http::Server::acceptConnection() {
  // Accept the connection
  auto acceptedSock = listenSocket.accept();

  if (acceptedSock) {
    // Keep-alive connections stay open between requests, so nothing else
    // bounds how many accumulate over time except this check.
    if (connections.size() >= static_cast<size_t>(maxConnections)) {
      BELL_LOG(warn, LOG_TAG, "At max connections ({}), rejecting new one",
               maxConnections);
      return;
    }

    auto setBlockingRes = acceptedSock->setBlocking(true);
    if (!setBlockingRes) {
      BELL_LOG(error, LOG_TAG, "Error setBlocking on accepted socket: {}",
               setBlockingRes.error());
      return;
    }

    int clientFd = acceptedSock->getFd();
    FD_SET(clientFd, &masterFdSet);
    BELL_LOG(debug, LOG_TAG, "Accepted connection");
    auto socket = std::make_shared<net::TCPSocket>(std::move(*acceptedSock));
    connections.push_back({
        socket,
        std::make_shared<net::SocketStream>(socket),
        std::chrono::steady_clock::now(),
        false,
    });
  } else {
    BELL_LOG(error, LOG_TAG, "Error accepting connection: {}",
             acceptedSock.error());
  }
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
  for (auto& connection : connections) {
    if (connection.socket->getFd() == fd) {
      // Mark the connection as closed
      connection.closed = true;
      return;
    }
  }
}

void http::Server::readFromClient(Connection& connection) {
  connection.lastActivity = std::chrono::steady_clock::now();

  auto reader = std::make_unique<http::Reader>(
      Direction::Request, connection.stream.get(), &readBuffer);
  auto readerRes = reader->readHeaders();

  if (!readerRes) {
    BELL_LOG(error, LOG_TAG, "Error reading headers: {}", readerRes.error());
    closeConnection(connection.socket->getFd());
    return;
  }

  const bool clientWantsKeepAlive = reader->keepAliveRequested();

  auto writer = std::make_unique<http::Writer>(Direction::Response,
                                               connection.stream.get());
  writer->setHeader("Connection", clientWantsKeepAlive ? "keep-alive" : "close");

  // Find the handler for the request
  auto handler = router.find(*reader->getMethod(), *reader->getPath());

  if (!handler) {
    notFoundHandler(reader, writer, {});
  } else {
    try {
      handler->first(reader, writer, handler->second);
    } catch (const std::exception& e) {
      BELL_LOG(error, LOG_TAG, "Error occured in the request handler: {}",
               e.what());
      (void)writer->writeResponseWithBody(500, {}, "Internal server error");
    }
  }

  if (!writer->hasWrittenHeaders() || !writer->hasWrittenBody()) {
    BELL_LOG(error, LOG_TAG, "Handler did not write response");
  }

  // A handler that doesn't read a POST body (or errors before doing so)
  // would otherwise leave those bytes in front of the next request on a
  // reused connection.
  auto bodyDrainedRes = reader->discardRemainingBody();

  const bool shouldKeepAlive = clientWantsKeepAlive && bodyDrainedRes.has_value() &&
                               writer->hasWrittenHeaders() &&
                               writer->hasWrittenBody();
  if (!shouldKeepAlive) {
    closeConnection(connection.socket->getFd());
  }
}

void http::Server::taskLoop() {
  fd_set readFdSet = masterFdSet;

  auto selectTV = bell::utils::millisecondsToTimeval(1000);

  maxFd = listenSocket.getFd();
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
  if (FD_ISSET(listenSocket.getFd(), &readFdSet)) {
    acceptConnection();
  }

  // Handle data from each connected client
  auto now = std::chrono::steady_clock::now();
  for (auto it = connections.begin(); it != connections.end();) {
    int clientFd = it->socket->getFd();
    if (FD_ISSET(clientFd, &readFdSet)) {
      readFromClient(*it);
    } else if (now - it->lastActivity >
               std::chrono::milliseconds(keepAliveIdleTimeoutMs)) {
      it->closed = true;
    }

    if (it->closed) {
      BELL_LOG(debug, LOG_TAG, "Closing connection");
      FD_CLR(it->socket->getFd(), &masterFdSet);
      it->socket->close();
      it = connections.erase(it);
    } else {
      ++it;
    }
  }
}
