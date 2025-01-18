#pragma once

#include <functional>  // for function
#include <memory>
#include <optional>
#include <string>         // for string
#include <unordered_map>  // for unordered_map
#include <vector>         // for vector
#include "bell/net/IpAddress.h"

namespace bell::mdns {
enum class EventType {
  ServiceAdded,          //< Service has been added
  ServiceRemoved,        //< Service has been removed
  ServiceResolved,       //< Service has been resolved
  ServiceResolveFailed,  //< Service resolution has failed
  AddressResolved,       //< Address has been resolved
  AddressResolveFailed   //< Address resolution has failed
};

class Browser {
 public:
  virtual ~Browser() = default;
  Browser() = default;

  // Disable copy operations, as we are managing instances via std::unique_ptr
  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;

  // Type for discovered MDNS record
  struct DiscoveredRecord {
    uint32_t interfaceIndex = 0;

    std::string name;
    std::string domain;
    std::string regType;

    std::string fullName;
    std::string hostname;
    uint16_t port = 0;

    bool serviceResolved = false;
    bool addressResolved = false;

    std::unordered_map<std::string, std::string> txtRecords;

    std::vector<net::IpAddress> addresses;

    bool operator==(const DiscoveredRecord& other) const {
      return name == other.name && regType == other.regType &&
             domain == other.domain && interfaceIndex == other.interfaceIndex;
    }

    inline void parseTXTRecords(const unsigned char* txtRecord,
                                uint16_t txtLen) {
      txtRecords.clear();

      int i = 0;

      while (i < txtLen) {
        // Get the length of the current key-value pair
        int length = txtRecord[i];
        i++;

        if (i + length > txtLen) {
          // This should not happen if the input is well-formed
          throw std::runtime_error("Invalid TXT record");
          break;
        }

        // Extract the key-value pair
        std::string pair(reinterpret_cast<const char*>(txtRecord + i), length);
        i += length;

        // Find the position of the '=' character
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
          // Split into key and value
          std::string key = pair.substr(0, pos);
          std::string value = pair.substr(pos + 1);
          txtRecords[key] = value;
        } else {
          // If there's no '=', then it's just a key with an empty value
          txtRecords[pair] = "";
        }
      }
    }
  };

  // Typedef for record callback
  using DiscoveryEventCallback =
      std::function<void(EventType, const DiscoveredRecord&)>;

  /**
   * @brief Processes mDNS events, by handling the implementation's specific event loop.
   * This function should be called periodically from a thread where you wish to receive mDNS events.
   *
   * @param timeoutMs The timeout in milliseconds for the event processing loop
   */
  virtual void processEvents(int timeoutMs = 1000) = 0;

  // stops discovery
  virtual void stopDiscovery() = 0;

  /**
   * @brief Resolves the service.  This function is called automatically if autoResolveService is set to true.
   *
   * @param service The service to resolve, as returned by the discovery event
   * @remark This function is non blocking and will call the callback when the service is resolved.
   */
  virtual void resolveService(const DiscoveredRecord& service) = 0;

  /**
   * @brief Resolves the service's address.  This function is called automatically if autoResolveAddresses is set to true.
   *
   * @param service The service to resolve, as returned by the discovery event
   * @remark This function is non blocking and will call the callback when the service is resolved.
   */
  virtual void resolveAddress(const DiscoveredRecord& service) = 0;

  /**
   * @brief Starts the mDNS service discovery process, and returns a unique pointer to the platform-specific browser instance.
   *
   * @param regType The service type to discover, eg "_http._tcp"
   * @param regDomain The domain to search in, eg "local". If empty, the default domain will be used.
   * @param interfaceIndex The network interface index to use for discovery. If 0, all interfaces will be used.
   * @param callback The callback to be called on discovery events
   * @param autoResolveService Whether to automatically resolve the service after discovery
   * @param autoResolveAddresses Whether to automatically resolve the addresses after service resolution
   * @param resolveIPv6 Whether to resolve IPv6 addresses during address resolution
   *
   * @remark After calling this function, the caller should call processEvents() periodically to process the mDNS events. The callback will be called on the same thread as processEvents().
   *
   * @return std::unique_ptr<MDNSBrowser> A unique pointer to the platform-specific browser instance
   */
  static std::unique_ptr<Browser> startDiscovery(
      const std::string& regType, const std::string& regDomain,
      int interfaceIndex, const DiscoveryEventCallback& onEvent,
      bool autoResolveService = true, bool autoResolveAddresses = true,
      bool resolveIPv6 = true);
};

}  // namespace bell::mdns

namespace bell {
using MDNSBrowser = mdns::Browser;
}
