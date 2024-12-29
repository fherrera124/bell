#include "bell/mdns/Service.h"

using namespace bell;

/**
 * MacOS implementation of mdns::Service.
 * @see https://developer.apple.com/documentation/dnssd/1804733-dnsserviceregister
 **/
std::unique_ptr<mdns::Service> mdns::Service::registerService(
    const std::string& serviceName, const std::string& serviceType,
    const std::string& serviceProto, const std::string& serviceHost,
    int servicePort, const std::map<std::string, std::string>& txtData) {
      return nullptr;
}
