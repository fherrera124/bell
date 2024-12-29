#include "bell/mdns/Browser.h"

using namespace bell;

std::unique_ptr<mdns::Browser> mdns::Browser::startDiscovery(
    const std::string& regType, const std::string& regDomain,
    int interfaceIndex, const DiscoveryEventCallback& onEvent,
    bool autoResolveService, bool autoResolveAddresses, bool resolveIpv6) {
  // TODO implement on Linux
  return nullptr;
}
