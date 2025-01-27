#pragma once

#include <string>

namespace bell::net {
/**
 * @brief Base pure socket class to be implemented by different socket types.
 *
 * This class provides a standard interface for socket operations, which can be
 * extended by different socket types (e.g., TCP, UDP). It defines essential
 * methods for opening, closing, reading from, writing to, and polling a socket,
 * as well as wrapping existing file descriptors.
 */
class Socket {
 public:
  Socket() = default;  ///< Default constructor.
  virtual ~Socket() =
      default;  ///< Virtual destructor for proper cleanup in derived classes.

  // Non-copyable
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  // Movable
  Socket(Socket&&) noexcept = default;
  Socket& operator=(Socket&&) noexcept = default;

  /**
   * @brief Set the blocking mode of the socket.
   *
   * @param blocking True to set the socket to blocking mode, false for non-blocking.
   */
  virtual void setBlocking(bool blocking) = 0;

  /**
   * @brief Set the timeout for socket operations.
   *
   * @param timeoutMs Timeout in milliseconds. A value of 0 indicates a non-blocking operation.
   */
  virtual void setTimeout(int timeoutMs) = 0;

  /**
   * @brief Wrap an existing file descriptor with this socket.
   *
   * This method allows an existing file descriptor (fd) to be wrapped and
   * treated as a socket within the application. This can be useful for integrating
   * sockets created by other means or from external sources.
   *
   * @param fd File descriptor to wrap.
   */
  virtual void wrapFd(int fd) = 0;

  /**
  * @brief Poll the socket for specific events.
  *
  * @param events Bitmask specifying the events to check for (e.g., readability, writability).
  * @param timeoutMs Timeout in milliseconds. A value of 0 indicates a non-blocking poll.
  * @return A bitmask indicating which events occurred, or 0 if the timeout expired.
  */
  virtual int poll(int events, int timeoutMs = 0) = 0;

  /**
   * @brief Write data to the socket.
   *
   * This method sends data from the provided buffer to the socket. The method
   * blocks until the data is sent or an error occurs. The return value indicates
   * the number of bytes successfully written.
   *
   * @param buf Pointer to the buffer containing the data to send.
   * @param len The number of bytes to write from the buffer.
   * @return The number of bytes successfully written.
   */
  virtual size_t write(const uint8_t* buf, size_t len) = 0;

  /**
   * @brief Read data from the socket.
   *
   * This method receives data from the socket and stores it in the provided buffer.
   * The method blocks until data is available or an error occurs. The return value
   * indicates the number of bytes successfully read.
   *
   * @param buf Pointer to the buffer where the received data will be stored.
   * @param len The maximum number of bytes to read into the buffer.
   * @return The number of bytes successfully read. A return value of 0 may indicate
   * that the connection was closed, while a value less than len could indicate that
   * no more data is currently available.
   */
  virtual size_t read(uint8_t* buf, size_t len) = 0;

  /**
   * @brief Bind the socket to a specific address and port.
   *
   * @param address A string representation of the address to bind to (e.g., "127.0.0.1").
   * @param port The port number to bind to.
   */
  virtual void bind(const std::string& address, uint16_t port) = 0;

  /**
   * @brief Get the local address and port of the socket.
   *
   * @return A string representation of the local address (e.g., "127.0.0.1:8080").
   */
  virtual std::string getLocalAddress() const = 0;

  /**
   * @brief Get the remote address and port of the connected socket.
   *
   * @return A string representation of the remote address (e.g., "192.168.1.1:12345").
   */
  virtual std::string getRemoteAddress() const = 0;

  /**
   * @brief Check if the socket is currently open.
   *
   * @return True if the socket is open, false otherwise.
   */
  virtual bool isOpen() = 0;

  /**
   * @brief Close the socket.
   *
   * This method closes the socket and releases any resources associated with it.
   * After calling this method, the socket is no longer usable until reopened.
   */
  virtual void close() = 0;

  /**
   * @brief Get the file descriptor associated with the socket.
   *
   * @return The file descriptor associated with the socket.
   */
  virtual int getFd() = 0;
};
}  // namespace bell::net

namespace bell {
using Socket = net::Socket;
}
