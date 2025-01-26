#include "bell/mdns/Browser.h"

// Standar includes
#include <algorithm>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

// Library includes
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/address.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/strlst.h>
#include <netinet/in.h>

// Bell includes
#include "bell/Logger.h"
#include "bell/utils/Utils.h"

using namespace bell;

namespace {
std::unordered_map<std::string, std::string> parseAvahiTxtValues(
    const std::string& avahiString) {
  std::unordered_map<std::string, std::string> txtValues;

  std::size_t start = 0;
  while (start < avahiString.length()) {
    // Find the starting quote of the key=value pair
    std::size_t quoteStart = avahiString.find('"', start);
    if (quoteStart == std::string::npos) {
      break;
    }

    // Find the ending quote of the key=value pair
    std::size_t quoteEnd = avahiString.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) {
      break;
    }

    // Extract the key=value substring
    std::string keyValue =
        avahiString.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    std::size_t equalsPos = keyValue.find('=');
    if (equalsPos != std::string::npos) {
      // Extract key and value
      std::string key = keyValue.substr(0, equalsPos);
      std::string value = keyValue.substr(equalsPos + 1);
      txtValues[key] = value;
    }

    // Move start forward
    start = quoteEnd + 1;
  }

  return txtValues;
}
}  // namespace

class implMDNSBrowser : public mdns::Browser {
 public:
  implMDNSBrowser(const std::string& regType, const std::string& regDomain,
                  int interfaceIndex, DiscoveryEventCallback onEvent,
                  bool autoResolveService = true, bool resolveIpv6 = true)
      : regType(regType),
        regDomain(regDomain),
        autoResolveService(autoResolveService),
        onEvent(std::move(onEvent)),
        avahiPoll(avahi_simple_poll_new()) {
    if (avahiPoll == nullptr) {
      throw std::runtime_error("Failed to create Avahi poll object");
    }

    int errorCode{};
    avahiClient = avahi_client_new(avahi_simple_poll_get(avahiPoll),
                                   static_cast<AvahiClientFlags>(0), nullptr,
                                   this, &errorCode);

    if (avahiClient == nullptr) {
      throw std::runtime_error(fmt::format("Failed to create Avahi client: {}",
                                           avahi_strerror(errorCode)));
    }

    serviceBrowser = avahi_service_browser_new(
        avahiClient, interfaceIndex == 0 ? AVAHI_IF_UNSPEC : interfaceIndex,
        resolveIpv6 ? AVAHI_PROTO_UNSPEC : AVAHI_PROTO_INET, regType.c_str(),
        regDomain.empty() ? nullptr : regDomain.c_str(),
        static_cast<AvahiLookupFlags>(0), avahiBrowseCallback, this);

    if (serviceBrowser == nullptr) {
      throw std::runtime_error(
          fmt::format("Failed to create Avahi service browser: {}",
                      avahi_strerror(avahi_client_errno(avahiClient))));
    }
  }

  // Delete copy constructor and copy assignment operator
  implMDNSBrowser(const implMDNSBrowser&) = delete;
  implMDNSBrowser& operator=(const implMDNSBrowser&) = delete;

  ~implMDNSBrowser() override {
    stopDiscovery();

    if (avahiClient) {
      avahi_client_free(avahiClient);
    }
    if (avahiPoll) {
      avahi_simple_poll_free(avahiPoll);
    }
  }

 private:
  const char* LOG_TAG = "AvahiMDNSBrowser";

  std::string regType;
  std::string regDomain;
  bool autoResolveService;
  bool resolveIpv6;
  DiscoveryEventCallback onEvent;

  AvahiClient* avahiClient = nullptr;
  AvahiServiceBrowser* serviceBrowser = nullptr;
  AvahiSimplePoll* avahiPoll = nullptr;

  std::vector<DiscoveredRecord> discoveredRecords;

  void resolveCallback(AvahiIfIndex interface, AvahiProtocol /*protocol*/,
                       AvahiResolverEvent event, const char* name,
                       const char* type, const char* domain,
                       const char* hostName, const AvahiAddress* address,
                       uint16_t port, AvahiStringList* txt,
                       AvahiLookupResultFlags /*flags*/) {
    std::string addressStr(AVAHI_ADDRESS_STR_MAX, '\0');

    auto matchingRecord = std::find_if(
        discoveredRecords.begin(), discoveredRecords.end(),
        [&name, &type, &domain, interface](const auto& record) {
          return record.interfaceIndex == static_cast<uint32_t>(interface) &&
                 record.name == name && record.regType == type &&
                 record.domain == domain;
        });

    if (matchingRecord == discoveredRecords.end()) {
      BELL_LOG(warn, LOG_TAG,
               "Service not found in discovered records, possibly already "
               "removed. Name={}, Type={}, Domain={}",
               name, type, domain);
      return;
    }

    if (event == AVAHI_RESOLVER_FOUND) {
      avahi_address_snprint(addressStr.data(), addressStr.size(), address);
      addressStr.erase(addressStr.find('\0'));

      matchingRecord->hostname = hostName;
      matchingRecord->fullName = fmt::format("{}.{}.{}", name, type, domain);
      matchingRecord->port = ntohs(port);
      matchingRecord->serviceResolved = true;

      auto* recordsString = avahi_string_list_to_string(txt);
      matchingRecord->txtRecords = parseAvahiTxtValues(recordsString);
      delete recordsString;

      onEvent(mdns::EventType::ServiceResolved, *matchingRecord);

      auto parsedAddress = net::IpAddress::fromString(addressStr);

      if (parsedAddress.has_value()) {
        matchingRecord->addresses.push_back(parsedAddress.value());
        onEvent(mdns::EventType::AddressResolved, *matchingRecord);
      } else {
        BELL_LOG(warn, LOG_TAG,
                 "Failed to parse address for service {}. Error: {}", name,
                 addressStr);
        onEvent(mdns::EventType::AddressResolveFailed, *matchingRecord);
      }
    } else {
      BELL_LOG(error, LOG_TAG, "Failed to resolve service. Name={}, errcode={}",
               name, avahi_strerror(avahi_client_errno(avahiClient)));

      onEvent(mdns::EventType::ServiceResolveFailed, *matchingRecord);
    }
  }

  void browseCallback(AvahiIfIndex interface, AvahiProtocol /*protocol*/,
                      AvahiBrowserEvent event, const char* name,
                      const char* type, const char* domain,
                      AvahiLookupResultFlags /*flags*/) {
    switch (event) {
      case AVAHI_BROWSER_FAILURE:
        throw std::runtime_error(
            fmt::format("Failed to browse for services, errcode={}",
                        avahi_strerror(avahi_client_errno(avahiClient))));
      case AVAHI_BROWSER_NEW:
      case AVAHI_BROWSER_REMOVE: {
        // Try to find the service in the list
        auto matchingRecord = std::find_if(
            discoveredRecords.begin(), discoveredRecords.end(),
            [&name, &type, &domain, interface](const auto& record) {
              return record.interfaceIndex ==
                         static_cast<uint32_t>(interface) &&
                     record.name == name && record.regType == type &&
                     record.domain == domain;
            });

        if (event == AVAHI_BROWSER_NEW) {
          if (matchingRecord == discoveredRecords.end()) {
            // Create a new record
            DiscoveredRecord record;
            record.interfaceIndex = static_cast<uint32_t>(interface);
            record.name = name;
            record.regType = type;
            record.domain = domain;
            discoveredRecords.push_back(record);

            matchingRecord = discoveredRecords.end() - 1;
          }

          // Notify about the event
          onEvent(mdns::EventType::ServiceAdded, *matchingRecord);

          if (autoResolveService) {
            resolveService(*matchingRecord);
          }
        } else if (matchingRecord != discoveredRecords.end()) {
          // Remove the record
          onEvent(mdns::EventType::ServiceRemoved, *matchingRecord);
          discoveredRecords.erase(matchingRecord);
        }
        break;
      }

      case AVAHI_BROWSER_CACHE_EXHAUSTED:
      case AVAHI_BROWSER_ALL_FOR_NOW:
        break;
    }
  }

  // Thin shim passing C-style browse callback to member function
  static void avahiBrowseCallback(AvahiServiceBrowser* /*browser*/,
                                  AvahiIfIndex interface,
                                  AvahiProtocol protocol,
                                  AvahiBrowserEvent event, const char* name,
                                  const char* type, const char* domain,
                                  AvahiLookupResultFlags flags,
                                  void* userdata) {
    auto* self = static_cast<implMDNSBrowser*>(userdata);
    self->browseCallback(interface, protocol, event, name, type, domain, flags);
  }

  // Thin shim passing C-style resolve callback to member function
  static void avahiResolveCallback(
      AvahiServiceResolver* /*resolver*/, AvahiIfIndex interface,
      AvahiProtocol protocol, AvahiResolverEvent event, const char* name,
      const char* type, const char* domain, const char* hostName,
      const AvahiAddress* address, uint16_t port, AvahiStringList* txt,
      AvahiLookupResultFlags flags, void* userdata) {
    auto* self = static_cast<implMDNSBrowser*>(userdata);
    self->resolveCallback(interface, protocol, event, name, type, domain,
                          hostName, address, port, txt, flags);
  }

  void processEvents(int timeoutMs) override {
    // Start Avahi thread poll
    if (avahi_simple_poll_iterate(avahiPoll, timeoutMs) < 0) {
      throw std::runtime_error("Failed to loop Avahi poll");
    }
  }

  void stopDiscovery() override {
    if (serviceBrowser) {
      avahi_service_browser_free(serviceBrowser);
      serviceBrowser = nullptr;
    }
  }

  void resolveService(const DiscoveredRecord& service) override {
    if (!(avahi_service_resolver_new(
            avahiClient, static_cast<int>(service.interfaceIndex),
            resolveIpv6 ? AVAHI_PROTO_UNSPEC : AVAHI_PROTO_INET,
            service.name.c_str(), service.regType.c_str(),
            service.domain.empty() ? nullptr : service.domain.c_str(),
            resolveIpv6 ? AVAHI_PROTO_UNSPEC : AVAHI_PROTO_INET,
            (AvahiLookupFlags)0, avahiResolveCallback, this))) {
      throw std::runtime_error("Failed to resolve service");
    }
  }

  void resolveAddress(const DiscoveredRecord& /*service*/) override {
    // There is no need to manually resolve the address in Avahi, as it is provided in the resolve callback
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
