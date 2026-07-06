#pragma once

#include <string>
#include <unordered_map>

#include "bell/Result.h"

namespace bell::mdns {
/**
 * @brief Abstract base class for a handle representing a registered mDNS service.
 */
class Advertiser {
 public:
  virtual ~Advertiser() = default;
  Advertiser() = default;

  // Disable copy operations, as we are managing instances via std::unique_ptr
  Advertiser(const Advertiser&) = delete;
  Advertiser& operator=(const Advertiser&) = delete;

  /**
   * @brief Stop advertising the service. Automatically called on destruction.
   */
  virtual void stopAdvertising() = 0;

  /**
   * @brief Update the advertised instance name and TXT records in place,
   * re-announcing the service so browsers observe the change.
   */
  virtual bell::Result<> update(
      const std::string& serviceName,
      const std::unordered_map<std::string, std::string>& txtRecords) = 0;
};
}  // namespace bell::mdns
