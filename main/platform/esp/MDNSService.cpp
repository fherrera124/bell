#include "bell/mdns/Service.h"

#include <cassert>
#include <functional>
#include <vector>

// Own includes
#include "bell/Logger.h"
#include "bell/utils/Semaphore.h"

using namespace bell;

/**
 * Avahi implementation of mdns::Service.
 * @see https://avahi.org/doxygen/html/
 * @remark This implementation does not handle collisions between services.
 **/
std::unique_ptr<mdns::Service> mdns::Service::registerService(
    const std::string& serviceName, const std::string& serviceType,
    const std::string& serviceProto, const std::string& serviceHost,
    int servicePort, const std::map<std::string, std::string>& txtData) {
  return {};
}
