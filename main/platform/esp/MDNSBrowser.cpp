#include "bell/mdns/Browser.h"

// Standar includes
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

// Library includes
#include "esp_netif_ip_addr.h"
#include "lwip/ip_addr.h"
#include "mdns.h"

// Bell includes
#include "bell/Logger.h"

using namespace bell;

class implMDNSBrowser : public mdns::Browser {
 public:
  implMDNSBrowser(std::string regType, std::string regDomain,
                  DiscoveryEventCallback onEvent)
      : regType(std::move(regType)),
        regDomain(std::move(regDomain)),
        onEvent(std::move(onEvent)) {
    // Extract the protocol from the regType
    serviceType = this->regType.substr(0, this->regType.find_first_of('.'));
    proto = this->regType.substr(this->regType.find_first_of('.') + 1);
  }

  // Delete copy constructor and copy assignment operator
  implMDNSBrowser(const implMDNSBrowser&) = delete;
  implMDNSBrowser& operator=(const implMDNSBrowser&) = delete;

  ~implMDNSBrowser() override { stopDiscovery(); }

 private:
  std::string regType;
  std::string regDomain;
  std::string proto;
  std::string serviceType;
  DiscoveryEventCallback onEvent;

  const int maxResults = 32;

  std::vector<DiscoveredRecord> cachedRecords;
  std::vector<DiscoveredRecord> receivedRecords;

  void parseResults(mdns_result_t* results) {
    mdns_result_t* r = results;
    mdns_ip_addr_t* a = nullptr;

    while (r) {
      DiscoveredRecord record;

      // Assign netif index
      record.interfaceIndex = esp_netif_get_netif_impl_index(r->esp_netif);

      if (r->instance_name) {
        record.name = r->instance_name;
        record.serviceResolved = true;
      }

      if (r->hostname) {
        record.hostname = r->hostname;
        record.port = r->port;
        record.addressResolved = true;
      }

      a = r->addr;
      while (a) {
        if (a->addr.type == IPADDR_TYPE_V4) {
          std::array<char, IP4ADDR_STRLEN_MAX> strCharData{};
          esp_ip4addr_ntoa(&a->addr.u_addr.ip4, strCharData.data(),
                           IP4ADDR_STRLEN_MAX);
          std::string ipStr(strCharData.data());
          auto ip = net::IpAddress::fromString(ipStr);

          if (ip.has_value()) {
            record.addresses.push_back(ip.value());
          }
        } else if (a->addr.type == IPADDR_TYPE_V6) {
          // TODO: Implement IPv6 support
          //   std::array<char, IP6ADDR_STRLEN_MAX> strCharData{};
          //   esp_ip6addr(&a->addr.u_addr.ip6, strCharData.data(),
          //                    IP6ADDR_STRLEN_MAX);
          //   std::string ipStr(strCharData.data());
          //   record.addresses.push_back(net::IpAddress::fromString(ipStr));
          // }
        }
        a = a->next;
      }

      // copy txt records int std::unordered_map
      for (int x = 0; x < r->txt_count; x++) {
        record.txtRecords.insert(
            {std::string(r->txt[x].key),
             std::string(&r->txt[x].value[0],
                         &r->txt[x].value[r->txt_value_len[x]])});
      }

      // Notify of new services
      if (std::find(cachedRecords.begin(), cachedRecords.end(), record) ==
          cachedRecords.end()) {
        onEvent(mdns::EventType::ServiceAdded, record);
        if (record.serviceResolved) {
          onEvent(mdns::EventType::ServiceResolved, record);
        }
        if (record.addressResolved) {
          onEvent(mdns::EventType::AddressResolved, record);
        }
      }

      receivedRecords.push_back(record);
      r = r->next;
    }

    // Notify of removed services
    for (auto& cachedRecord : cachedRecords) {
      if (std::find(receivedRecords.begin(), receivedRecords.end(),
                    cachedRecord) == receivedRecords.end()) {
        onEvent(mdns::EventType::ServiceRemoved, cachedRecord);
      }
    }

    cachedRecords.assign(receivedRecords.begin(), receivedRecords.end());
  }

  void processEvents(int timeoutMs) override {
    // Start Avahi thread poll
    mdns_result_t* results = nullptr;
    esp_err_t err = mdns_query_ptr(serviceType.c_str(), proto.c_str(),
                                   timeoutMs, maxResults, &results);
    if (err) {
      BELL_LOG(
          error, "MDNSBrowser",
          "Failed to query mdns services. Service type: {}, proto: {}, err: {}",
          serviceType, proto, static_cast<int>(err));
      throw std::runtime_error("Could not query mdns services");
    }

    mdns_query_results_free(results);
  }

  void stopDiscovery() override {}

  void resolveService(const DiscoveredRecord& /*service*/) override {
    // There is no need to manually resolve the service with espressif mdns, it is done automatically
  }

  void resolveAddress(const DiscoveredRecord& /*service*/) override {
    // There is no need to manually resolve the address with espressif mdns, it is done automatically
  }
};

std::unique_ptr<mdns::Browser> mdns::Browser::startDiscovery(
    const std::string& regType, const std::string& regDomain,
    int /*interfaceIndex*/, const DiscoveryEventCallback& onEvent,
    bool /*autoResolveService*/, bool /*autoResolveAddresses*/,
    bool /*resolveIpv6*/) {
  return std::make_unique<implMDNSBrowser>(regType, regDomain, onEvent);
}
