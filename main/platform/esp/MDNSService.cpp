#include "bell/mdns/Service.h"

// System includes
#include <cassert>
#include <string>
#include <utility>
#include <vector>

// Own includes
#include "mdns.h"

using namespace bell;

// Espressif implementation of mdns::Service
class implMDNSService : public mdns::Service {
 public:
  implMDNSService(std::string type, std::string proto,
                  std::vector<mdns_txt_item_t> txtItems)
      : type(std::move(type)),
        proto(std::move(proto)),
        txtItems(std::move(txtItems)) {}
  ~implMDNSService() override { unregisterService(); }

  // Delete copy constructor and copy assignment operator
  implMDNSService(const implMDNSService&) = delete;
  implMDNSService& operator=(const implMDNSService&) = delete;

  void unregisterService() override {
    mdns_service_remove(type.c_str(), proto.c_str());
  }

 private:
  std::string type;
  std::string proto;
  std::vector<mdns_txt_item_t> txtItems;
};

/**
 * ESP32 implementation of MDNSService
 * @see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mdns.html
 **/
std::unique_ptr<mdns::Service> mdns::Service::registerService(
    const std::string& serviceName, const std::string& serviceType,
    const std::string& serviceProto, const std::string& serviceHost,
    int servicePort, const std::map<std::string, std::string>& txtData) {
  std::vector<mdns_txt_item_t> txtItems;
  txtItems.reserve(txtData.size());
  for (const auto& data : txtData) {
    mdns_txt_item_t item;
    item.key = data.first.c_str();
    item.value = data.second.c_str();
    txtItems.push_back(item);
  }

  mdns_service_add(serviceName.c_str(),  /* instance_name */
                   serviceType.c_str(),  /* service_type */
                   serviceProto.c_str(), /* proto */
                   servicePort,          /* port */
                   txtItems.data(),      /* txt */
                   txtItems.size()       /* num_items */
  );
  return std::make_unique<implMDNSService>(serviceType, serviceProto, txtItems);
}
