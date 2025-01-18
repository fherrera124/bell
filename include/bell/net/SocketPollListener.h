#pragma once

// System includes
#include <sys/poll.h>
#include <functional>
#include <unordered_map>
#include <vector>

namespace bell::net {
class SocketPollListener {
 public:
  // Default constructor
  SocketPollListener() = default;

  using SocketEventCallback = std::function<void(short)>;

  /**
   * @brief Registers a socket with the poll listener.
   *
   * @param fd File descriptor of the socket
   * @param eventMask Bitmask of events to listen for, e.g., POLLIN, POLLOUT
   * @param eventCallback Callback function to call when the event occurs
   */
  void registerSocket(int fd, short eventMask,
                      const SocketEventCallback& eventCallback);

  // Polls the registered sockets for events
  void poll(int timeoutMs = -1);

  // Unregisters a socket from the poll listener
  void unregisterSocket(int fd);

 private:
  // keeps reference to socket we're listening to events from
  std::unordered_map<int, SocketEventCallback> handlers;
  std::vector<pollfd> fds{};
};
}  // namespace bell::net

namespace bell {
using SocketPollListener = net::SocketPollListener;
}
