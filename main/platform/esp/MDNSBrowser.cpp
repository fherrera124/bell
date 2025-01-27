#include "bell/mdns/Browser.h"

// Standar includes
#include <algorithm>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

using namespace bell;


std::unique_ptr<mdns::Browser> mdns::Browser::startDiscovery(
    const std::string& regType, const std::string& regDomain,
    int interfaceIndex, const DiscoveryEventCallback& onEvent,
    bool autoResolveService, bool /*autoResolveAddresses*/, bool resolveIpv6) {
  return {};
}
