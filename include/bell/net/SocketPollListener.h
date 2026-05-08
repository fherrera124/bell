#pragma once

#include <sys/poll.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "bell/net/Socket.h"

namespace bell::net {
class SocketPollListener {
 public:
  enum class Event : int {
    Readable = POLLIN,
    Writeable = POLLOUT,
    Error = POLLERR,
    Hangup = POLLHUP,
    Priority = POLLPRI,
  };

  using EventCallback = std::function<void(Socket&)>;

  /**
   * @brief RAII registration handle. Destructor removes the (fd, event)
   * subscription from the listener and releases the socket reference.
   */
  class Registration {
   public:
    Registration() = default;
    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&& other) noexcept;
    Registration& operator=(Registration&& other) noexcept;
    ~Registration() { reset(); }
    void reset();
    bool valid() const { return listener_ != nullptr; }

   private:
    friend class SocketPollListener;
    Registration(SocketPollListener* l, std::shared_ptr<Socket> s, Event e)
        : listener_(l), socket_(std::move(s)), event_(e) {}
    SocketPollListener* listener_ = nullptr;
    std::shared_ptr<Socket> socket_;
    Event event_{};
  };


  // When registerWakeSocket is set to true, an UDP socket used to wake up the poll early is registered
  SocketPollListener(bool registerWakeSocket = false);
  ~SocketPollListener();
  SocketPollListener(const SocketPollListener&) = delete;
  SocketPollListener& operator=(const SocketPollListener&) = delete;

  /**
   * @brief Start watching a socket for a given event.
   *
   * The returned Registration owns the subscription; destroying it removes
   * the (fd, event) entry. The Registration also holds a shared_ptr to the
   * socket, keeping it alive for the duration of the subscription.
   */
  [[nodiscard]] Registration watch(std::shared_ptr<Socket> socket,
                                   Event polledEvent, EventCallback onEvent);

  /**
   * @brief Poll all registered sockets.
   *
   * @param timeout nullopt = block until IO or wake(); Some(d) = block up to d.
   */
  void poll(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

  /// Interrupt a blocking poll() from another thread. Zero-latency:
  /// sends a byte to a loopback UDP self-socket that poll() also
  /// watches for POLLIN.
  void wake();

 private:
  struct Handler {
    std::shared_ptr<Socket> socket;
    std::unordered_map<Event, EventCallback> callbacks;
  };

  // Caller must hold pollMutex.
  void updateFdListLocked();
  void drainWakeFd();
  void unregister(int fd, Event event);

  std::unordered_map<int, Handler> handlers;
  std::vector<pollfd> fds;
  std::mutex pollMutex;

  // Loopback UDP socket used as a cross-thread wake channel. Bound to
  // 127.0.0.1:wakePort_ at construction; wake() sendto's a byte at its
  // own address, poll() drains it. One fd, zero latency, portable
  // across POSIX and lwIP (ESP-IDF).
  int wakeFd_ = -1;
  uint16_t wakePort_ = 0;  // host byte order
};
}  // namespace bell::net

namespace bell {
using SocketPollListener = net::SocketPollListener;
using PollEvent = net::SocketPollListener::Event;
}  // namespace bell
