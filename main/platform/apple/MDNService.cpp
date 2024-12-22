#include "bell/mdns/Service.h"

// System includes
#include <string>
#include <utility>

// Library includes
#include "dns_sd.h"

using namespace bell;

// MacOS implementation of mdns::Service, using the Apple Bonjour API
class implMDNSService : public mdns::Service {
 public:
  implMDNSService(DNSServiceRef* service) : service(service) {}
  ~implMDNSService() override { unregisterService(); }

  // Delete copy constructor and copy assignment operator
  implMDNSService(const implMDNSService&) = delete;
  implMDNSService& operator=(const implMDNSService&) = delete;

  void unregisterService() override {
    // Deallocate the service reference
    if (service != nullptr) {
      DNSServiceRefDeallocate(*service);
      service = nullptr;
    }
  }

 private:
  DNSServiceRef* service;  // Pointer to the service reference
};

/**
 * MacOS implementation of mdns::Service.
 * @see https://developer.apple.com/documentation/dnssd/1804733-dnsserviceregister
 **/
std::unique_ptr<mdns::Service> mdns::Service::registerService(
    const std::string& serviceName, const std::string& serviceType,
    const std::string& serviceProto, const std::string& serviceHost,
    int servicePort, const std::map<std::string, std::string>& txtData) {
  DNSServiceRef* ref = new DNSServiceRef();
  TXTRecordRef txtRecord;

  TXTRecordCreate(&txtRecord, 0, nullptr);
  for (const auto& data : txtData) {
    // Convert the key-value pair to a TXT record
    TXTRecordSetValue(&txtRecord, data.first.c_str(), data.second.size(),
                      data.second.c_str());
  }

  DNSServiceRegister(ref, 0, 0, serviceName.c_str(),
                     (serviceType + "." + serviceProto).c_str(), nullptr,
                     serviceHost.empty() ? nullptr : serviceHost.c_str(),
                     htons(servicePort), TXTRecordGetLength(&txtRecord),
                     TXTRecordGetBytesPtr(&txtRecord), nullptr, nullptr);

  // Free the TXT record
  TXTRecordDeallocate(&txtRecord);

  return std::make_unique<implMDNSService>(ref);
}
