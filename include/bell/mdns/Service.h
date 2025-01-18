#pragma once

#include <map>
#include <memory>

namespace bell::mdns {
class Service {
 public:
  Service() = default;
  virtual ~Service() = default;

  /**
   * @brief Register a service with the mDNS server.
   *
   * @param serviceName MDNS service name
   * @param serviceType MDNS service type
   * @param serviceProto MDNS service protocol
   * @param serviceHost Service hostname. If empty, the local hostname will be used.
   * @param servicePort service port
   * @param txtData additional TXT data to include in the service registration, as key-value pairs
   *
   * @return std::unique_ptr<Service> Pointer to the registered service. The service will be automatically unregistered when the object is destroyed.
   */
  static std::unique_ptr<Service> registerService(
      const std::string& serviceName, const std::string& serviceType,
      const std::string& serviceProto, const std::string& serviceHost,
      int servicePort, const std::map<std::string, std::string>& txtData);

  /**
   * @brief Unregister the service from the mDNS server.
   */
  virtual void unregisterService() = 0;
};
}  // namespace bell::mdns

namespace bell {
using MDNSService = mdns::Service;
}
