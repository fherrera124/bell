#pragma once

#include <sys/socket.h>
#include <string>

namespace bell::io::SocketUtils {
struct ResolvedAddress {
  sockaddr_storage addr;  // Can store either IPv4 or IPv6 address
  socklen_t addrLen;      // Length of the sockaddr structure
  int family;             // Address family: AF_INET or AF_INET6

  void setPort(int port);
};

/**
 * @brief Resolve the provided hostname to an IP address. In case the hostname is already an IP address, it is directly stored in the ResolvedAddress structure.
 * 
 * @param hostname The hostname or IP address to resolve.
 * @param sockType The socket type (e.g., SOCK_STREAM, SOCK_DGRAM).
 * @param family The address family (e.g., AF_INET, AF_INET6). Default is AF_UNSPEC, which allows both IPv4 and IPv6.
 * @return ResolvedAddress structure containing the resolved IP address.
 */
ResolvedAddress resolveDomain(const std::string& hostname, int sockType,
                              int family = AF_UNSPEC);

/**
 * @brief Convert milliseconds to a timeval structure.
 * 
 * @param ms The time in milliseconds.
 * @return timeval structure representing the time in milliseconds.
 */
struct timeval msToTimeval(int ms);
};  // namespace bell::io::SocketUtils