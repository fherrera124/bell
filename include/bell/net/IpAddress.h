#pragma once

#include <sys/socket.h>
#include <optional>
#include <string>

namespace bell::net {
class IpAddress {
 public:
  enum class Type { IPv4, IPv6, Unknown };

  // Default constructor
  IpAddress();

  // Create an address from a sockaddr structure. Should fill in the type and storage fields.
  IpAddress(const sockaddr* addr);

  // Returns the address in a string format. For IPv4, this is the dotted decimal format. For IPv6, this is the colon-separated format.
  std::string toString(bool includePort = true) const;

  // Returns the address type
  Type getType() const;

  // Returns the sockaddr structure pointer, for use with socket functions
  const sockaddr* getSockAddrPtr() const;

  // Returns the size of the sockaddr structure
  socklen_t getSockAddrLen() const;

  // Sets the optional port for the address.
  void setPort(uint16_t port);

  // Returns the inet family of the address
  int getFamily() const;

  /**
   * @brief Create an address from the string representation of an IP address.
   *
   * @param addrStr The string representation of the IP address. This can be either an IPv4 or IPv6 address.
   *
   * @remark This function does not perform any DNS resolution. Use resolveDomain() for that.
   * @return std::optional<Address> The address structure, or std::nullopt if the address string is invalid.
   */
  static std::optional<IpAddress> fromString(const std::string& addrStr);

  /**
   * @brief Resolve the provided hostname to an IP address. In case the hostname is already an IP address, it is directly stored in the Address structure.
   */
  static IpAddress resolveDomain(const std::string& hostname, int sockType,
                                 int family = AF_UNSPEC);

 private:
  Type addressType = Type::Unknown;
  sockaddr_storage storage{};
  socklen_t addrLen = 0;
  std::optional<uint16_t> port{};
};
}  // namespace bell::net
