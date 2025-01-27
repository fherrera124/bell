#include "bell/mdns/Browser.h"

// Standar includes
#include <array>
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
                  int interfaceIndex, DiscoveryEventCallback onEvent,
                  bool autoResolveService = true, bool resolveIpv6 = true)
      : regType(std::move(regType)),
        regDomain(std::move(regDomain)),
        onEvent(std::move(onEvent)) {
    // Extract the protocol from the regType
    serviceType = regType.substr(0, regType.find_first_of('.'));
    proto = regType.substr(regType.find_first_of('.') + 1);
  }

  // Delete copy constructor and copy assignment operator
  implMDNSBrowser(const implMDNSBrowser&) = delete;
  implMDNSBrowser& operator=(const implMDNSBrowser&) = delete;

  ~implMDNSBrowser() override { stopDiscovery(); }

 private:
  const char* LOG_TAG = "EspressifMDNSBrowser";

  std::string regType;
  std::string regDomain;
  std::string proto;
  std::string serviceType;
  DiscoveryEventCallback onEvent;

  const int maxResults = 32;

  std::vector<DiscoveredRecord> discoveredRecords;

  std::vector<DiscoveredRecord> parseResults(mdns_result_t* results) {
    mdns_result_t* r = results;
    mdns_ip_addr_t* a = nullptr;
    std::vector<DiscoveredRecord> records;

    while (r) {
      DiscoveredRecord record;

      record.interfaceIndex = r->esp_netif

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
          record.addresses.push_back(net::IpAddress::fromString(ipStr));
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

      records.push_back(record);
      r = r->next;
    }

    publishDiscovered();
  }

  void processEvents(int timeoutMs) override {
    // Start Avahi thread poll
    mdns_result_t* results = nullptr;
    esp_err_t err = mdns_query_ptr(serviceType.c_str(), proto.c_str(),
                                   timeoutMs, maxResults, &results);
    if (err) {
      throw std::runtime_error("Could not query mdns services");
    }

    processResults(results);

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
    int interfaceIndex, const DiscoveryEventCallback& onEvent,
    bool autoResolveService, bool /*autoResolveAddresses*/, bool resolveIpv6) {
  return std::make_unique<implMDNSBrowser>(regType, regDomain, interfaceIndex,
                                           onEvent, autoResolveService,
                                           resolveIpv6);
}
