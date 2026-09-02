// Own header
#include "bell/mdns/Manager.h"

#ifndef BELL_DISABLE_MDNS

// Standard includes
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <new>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Bell includes
#include "bell/Logger.h"
#include "bell/Result.h"
#include "bell/mdns/Error.h"
#include "bell/net/IpAddress.h"

// Library includes
#include "bell/utils/Task.h"

// Espressif includes
#include "esp_log.h"
#include "mdns.h"

using namespace bell::mdns;

namespace {

constexpr const char* dispatcherLogTag = "EspressifMdnsBrowseDispatcher";
constexpr size_t maxQueuedBrowseResults = 64;

void logBrowseDrop(const char* reason) {
  static std::atomic<uint32_t> drops{0};
  const uint32_t total = drops.fetch_add(1, std::memory_order_relaxed) + 1;
  if (total == 1 || (total % 64) == 0) {
    ESP_LOGW(dispatcherLogTag, "Dropped %lu mDNS browse update(s), reason=%s",
             static_cast<unsigned long>(total), reason);
  }
}

// Copy of a single browser result
struct BrowseResult {
  std::string serviceType;  // "_service._proto", matches the browse regType
  std::string instanceName;
  std::string hostname;
  uint16_t port = 0;
  uint32_t ttl = 0;
  int netifIndex = 0;
  std::unordered_map<std::string, std::string> txtRecords;
  std::optional<bell::net::IpAddress> ipv4;
};

BrowseResult snapshotResult(mdns_result_t* r) {
  BrowseResult out;
  if (r->service_type) {
    out.serviceType = r->service_type;
  }
  if (r->proto) {
    if (!out.serviceType.empty()) {
      out.serviceType += ".";
    }
    out.serviceType += r->proto;
  }
  if (r->instance_name) {
    out.instanceName = r->instance_name;
  }
  if (r->hostname) {
    out.hostname = r->hostname;
  }
  out.port = r->port;
  out.ttl = r->ttl;
  if (r->esp_netif) {
    out.netifIndex = esp_netif_get_netif_impl_index(r->esp_netif);
  }

  for (size_t x = 0; x < r->txt_count; x++) {
    if (!r->txt[x].key) {
      continue;
    }
    const char* value = r->txt[x].value ? r->txt[x].value : "";
    out.txtRecords.insert({std::string(r->txt[x].key),
                           std::string(value, value + r->txt_value_len[x])});
  }

  for (mdns_ip_addr_t* a = r->addr; a; a = a->next) {
    if (a->addr.type == IPADDR_TYPE_V4) {
      std::array<char, IP4ADDR_STRLEN_MAX> strCharData{};
      esp_ip4addr_ntoa(&a->addr.u_addr.ip4, strCharData.data(),
                       IP4ADDR_STRLEN_MAX);
      out.ipv4 = bell::net::IpAddress::fromString(strCharData.data());
    }
    // TODO: Implement IPv6 support
  }

  return out;
}

}  // namespace

// Hands browse updates from the mdns service task to a bell-owned task, so
// user callbacks run with the same threading and stack guarantees the query
// executor used to provide.
class BrowseDispatcher : public bell::Task {
 public:
  BrowseDispatcher()
      : bell::Task("mdns_browse_dispatch", 1024 * 8, 1,
                   bell::utils::TaskCore::Core0, /*espStackOnPsram=*/false) {
    if (startTask()) {
      activeDispatcher = this;
    } else {
      ESP_LOGE(dispatcherLogTag, "Failed to start mDNS browse dispatcher task");
    }
  }

  ~BrowseDispatcher() {
    if (activeDispatcher == this) {
      activeDispatcher = nullptr;
    }
    stopTask();
  }

  using ResultHandler = std::function<void(const BrowseResult&)>;

  void registerHandler(const std::string& regType, ResultHandler handler) {
    std::scoped_lock lock(dispatchMutex);
    handlers[regType] = std::move(handler);
  }

  // Blocks until an in-flight dispatch has finished, so after return no
  // handler for this type can still be running
  void unregisterHandler(const std::string& regType) {
    std::scoped_lock lock(dispatchMutex);
    handlers.erase(regType);
  }

  // Called from the mdns service task. Keep this bounded and quick: if this
  // queue grows without limit, normal C++ heap allocations here can starve the
  // mDNS task and make the component's action queue stop draining.
  void enqueue(BrowseResult&& result) {
    {
      std::scoped_lock lock(queueMutex);
      if (queue.size() >= maxQueuedBrowseResults) {
        queue.pop_front();
        logBrowseDrop("dispatcher backlog");
      }
      queue.push_back(std::move(result));
    }
    queueCv.notify_one();
  }

  // Single process-wide instance; the component's notifier callback carries
  // no user context, so it reaches the dispatcher through this pointer
  static inline BrowseDispatcher* activeDispatcher = nullptr;

 protected:
  void wakeTask() override {
    {
      std::scoped_lock lock(queueMutex);
      wakeSignal = true;
    }
    queueCv.notify_one();
  }

  void taskLoop() override {
    BrowseResult result;
    {
      std::unique_lock lock(queueMutex);
      queueCv.wait(lock, [this] { return wakeSignal || !queue.empty(); });
      if (queue.empty()) {
        // stopTask() wake; runTask() observes the cleared running flag
        wakeSignal = false;
        return;
      }
      result = std::move(queue.front());
      queue.pop_front();
    }

    std::scoped_lock lock(dispatchMutex);
    auto it = handlers.find(result.serviceType);
    if (it != handlers.end()) {
      it->second(result);
    }
  }

 private:
  std::mutex queueMutex;
  std::condition_variable queueCv;
  bool wakeSignal = false;
  std::deque<BrowseResult> queue;

  std::mutex dispatchMutex;
  std::unordered_map<std::string, ResultHandler> handlers;
};

namespace {

// One result per call, next points to the sibling
void browseNotifier(mdns_result_t* result) {
  auto* dispatcher = BrowseDispatcher::activeDispatcher;
  if (!dispatcher || !result) {
    return;
  }

  try {
    dispatcher->enqueue(snapshotResult(result));
  } catch (const std::bad_alloc&) {
    logBrowseDrop("out of memory");
  } catch (const std::exception& e) {
    logBrowseDrop(e.what());
  } catch (...) {
    logBrowseDrop("unknown exception");
  }
}

}  // namespace

class EspressifMdnsBrowser : public Browser {
 public:
  // Constructor
  EspressifMdnsBrowser(std::shared_ptr<BrowseDispatcher> dispatcher)
      : dispatcher(std::move(dispatcher)) {}

  ~EspressifMdnsBrowser() { stopDiscovery(); }

  bell::Result<> browse(const std::string& regType,
                        const DiscoveryEventCallback& onEvent) {
    // Ensure no previous browse is active
    stopDiscovery();

    this->regType = regType;
    this->regService = regType.substr(0, regType.find_first_of('.'));
    this->regProto = regType.substr(regType.find_first_of('.') + 1);
    this->onEvent = onEvent;

    // Handler registered first so no result of the fresh browse is missed
    dispatcher->registerHandler(
        regType, [this](const BrowseResult& r) { this->handleResult(r); });

    if (!mdns_browse_new(regService.c_str(), regProto.c_str(),
                         browseNotifier)) {
      BELL_LOG(error, LOG_TAG, "Failed to start mDNS browse for {}", regType);
      dispatcher->unregisterHandler(regType);
      this->regType.clear();
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_discovery_failed);
    }

    return {};
  }

  bell::Result<> resolveService(const ServiceRecord& service) override {
    (void)service;
    BELL_LOG(warn, LOG_TAG,
             "No need to manually resolve the services on Espressif platforms, "
             "the mDNS service is automatically resolved.");
    return {};
  }

  bell::Result<> resolveAddress(const ServiceRecord& service) override {
    (void)service;
    BELL_LOG(warn, LOG_TAG,
             "No need to manually resolve addresses on Espressif platforms, "
             "the mDNS service's address is automatically resolved.");
    return {};
  }

  void stopDiscovery() override {
    if (!regType.empty()) {
      // Unregister first
      dispatcher->unregisterHandler(regType);
      (void)mdns_browse_delete(regService.c_str(), regProto.c_str());
      regType.clear();
    }
  }

 private:
  const char* LOG_TAG = "EspressifMdnsBrowser";

  // Pointer to the event callback
  DiscoveryEventCallback onEvent = {};

  // Pointer to the browse dispatcher
  std::shared_ptr<BrowseDispatcher> dispatcher;

  // Service type and protocol
  std::string regService;
  std::string regProto;
  std::string regType;  // regService.regProto

  // Set of discovered services
  std::set<ServiceRecord> recordsCache{};

  // Fold one browse update into the cache and emit the resulting events
  void handleResult(const BrowseResult& result) {
    if (!onEvent) {
      return;
    }

    ServiceRecord service(result.instanceName, result.serviceType, "",
                          result.netifIndex);

    if (!result.hostname.empty()) {
      // Service is resolved
      service.hostname = result.hostname;
      service.port = result.port;
      service.txtRecords = result.txtRecords;
      service.serviceResolved = true;
    }

    service.ipv4 = result.ipv4;

    if (!service.ipv4) {
      // Ugly fix for espressif mdns sometimes missing an address when two instances of the same service are registered
      for (auto& it : recordsCache) {
        if (it.hostname == service.hostname && it.ipv4.has_value()) {
          service.ipv4 = it.ipv4;
          break;
        }
      }
    }

    bool freshRecord = false;

    if (recordsCache.find(service) == recordsCache.end()) {
      if (result.ttl == 0) {
        // Goodbye for a service that was never surfaced
        return;
      }

      // New service, notify the event callback
      DiscoveryEvent event{
          .type = EventType::Added,
          .service = service,
          .error = {},
      };
      onEvent(event);

      // Insert the service into the cache
      recordsCache.insert(service);

      freshRecord = true;
    }

    auto it = recordsCache.find(service);

    if (result.ttl == 0) {
      // Service is being removed
      DiscoveryEvent event{
          .type = EventType::Removed,
          .service = service,
          .error = {},
      };
      onEvent(event);

      // Remove the service from the cache
      recordsCache.erase(it);
      return;
    }

    bool serviceUpdated = false;
    if ((!it->serviceResolved && service.serviceResolved) ||
        (it->serviceResolved && freshRecord)) {
      // Service is resolved, notify the event callback
      DiscoveryEvent event{
          .type = EventType::Resolved,
          .service = service,
          .error = {},
      };
      onEvent(event);

      // Update the service in the cache
      serviceUpdated = true;
    }

    if ((!it->ipv4.has_value() && service.ipv4.has_value()) ||
        (it->ipv4.has_value() && freshRecord)) {
      // Address is resolved, notify the event callback
      DiscoveryEvent event{
          .type = EventType::AddressResolved,
          .service = service,
          .error = {},
      };
      onEvent(event);

      // Update the service in the cache
      serviceUpdated = true;
    }

    if (serviceUpdated && !freshRecord) {
      // Update the service in the cache
      recordsCache.erase(it);
      recordsCache.insert(service);
    }
  }
};

class EspressifMDNAdvertiser : public Advertiser {
 public:
  EspressifMDNAdvertiser() = default;
  ~EspressifMDNAdvertiser() override { stopAdvertising(); }

  // Remoce the copy constructor and assignment operator
  EspressifMDNAdvertiser(const EspressifMDNAdvertiser&) = delete;
  EspressifMDNAdvertiser& operator=(const EspressifMDNAdvertiser&) = delete;

  bell::Result<> advertise(
      const std::string& serviceName, const std::string& serviceType,
      uint16_t port,
      const std::unordered_map<std::string, std::string>& txtRecords) {
    // Create a new mDNS service
    this->regService = serviceType.substr(0, serviceType.find_first_of('.'));
    this->regProto = serviceType.substr(serviceType.find_first_of('.') + 1);

    txtItems.reserve(txtRecords.size());

    // Parse the TXT records
    for (const auto& data : txtRecords) {
      mdns_txt_item_t item;
      item.key = data.first.c_str();
      item.value = data.second.c_str();
      txtItems.push_back(item);
    }

    auto res = mdns_service_add(serviceName.c_str(), this->regService.c_str(),
                                this->regProto.c_str(), port, txtItems.data(),
                                txtItems.size());

    if (res != ESP_OK) {
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    return {};
  }

  bell::Result<> update(
      const std::string& serviceName,
      const std::unordered_map<std::string, std::string>& txtRecords) override {
    if (regService.empty()) {
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    // Rename the existing service rather than adding a new one
    auto res = mdns_service_instance_name_set(
        regService.c_str(), regProto.c_str(), serviceName.c_str());
    if (res != ESP_OK) {
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    std::vector<mdns_txt_item_t> items;
    items.reserve(txtRecords.size());
    for (const auto& data : txtRecords) {
      mdns_txt_item_t item;
      item.key = data.first.c_str();
      item.value = data.second.c_str();
      items.push_back(item);
    }

    res = mdns_service_txt_set(regService.c_str(), regProto.c_str(),
                               items.data(), items.size());
    if (res != ESP_OK) {
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    return {};
  }

  void stopAdvertising() override {
    if (!regService.empty()) {
      (void)mdns_service_remove(regService.c_str(), regProto.c_str());
      regService.clear();
      regProto.clear();
      txtItems.clear();
    }
  }

 private:
  std::string regService;
  std::string regProto;
  std::vector<mdns_txt_item_t> txtItems;
};

class EspressifMDNSManager : public Manager {
 public:
  // mdns_service_add() hard-requires both of these to have already
  // happened (confirmed by reading mdns_responder.c directly:
  // mdns_service_add() itself checks `s_server` - ESP_ERR_INVALID_STATE
  // if missing - and mdns_service_add_for_host_base() separately checks
  // `s_server->hostname` - ESP_ERR_INVALID_ARG if missing) - neither was
  // being called anywhere in this codebase, so advertise() was silently
  // failing on every real device (the mDNS service never actually got
  // registered, so nothing could ever discover it) - confirmed on real
  // JC3248W535 hardware: ZeroConf's own HTTP server came up fine, but the
  // device never appeared in the Spotify app.
  EspressifMDNSManager() {
    auto ret = mdns_init();
    if (ret != ESP_OK) {
      BELL_LOG(error, LOG_TAG, "mdns_init failed: {}", (int)ret);
    }
  }

  bell::Result<std::unique_ptr<Browser>> browse(
      const std::string& serviceType, const std::string& /*serviceDomain*/,
      int /*interfaceIndex*/, const Browser::DiscoveryEventCallback& onEvent,
      bool /*autoResolveService */, bool /*autoResolveAddresses */,
      bool /*resolveIPv6*/) override {
    auto dispatcher = browseDispatcher.lock();
    if (!dispatcher) {
      dispatcher = std::make_shared<BrowseDispatcher>();
      browseDispatcher = dispatcher;
    }
    auto browser = std::make_unique<EspressifMdnsBrowser>(dispatcher);
    auto res = browser->browse(serviceType, onEvent);

    if (!res) {
      return nonstd::make_unexpected(res.error());
    }

    return browser;
  }

  bell::Result<std::unique_ptr<Advertiser>> advertise(
      const std::string& serviceName, const std::string& serviceType,
      const std::string& /*serviceDomain*/, const std::string& /*serviceHost*/,
      uint16_t port,
      const std::unordered_map<std::string, std::string>& txtRecords,
      int /*interfaceIndex*/) override {
    // The mDNS hostname (the ".local" name a client resolves to an IP) and
    // default instance name are device-wide, not per-service - reusing
    // serviceName here matches the usual ZeroConf convention of the same
    // name for both. Same fix already validated in our other ESP32
    // project's own bell fork (MDNSService.cpp's ensureResponderStarted()).
    auto hostnameRet = mdns_hostname_set(serviceName.c_str());
    if (hostnameRet != ESP_OK) {
      BELL_LOG(error, LOG_TAG, "mdns_hostname_set failed: {}",
              (int)hostnameRet);
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }
    auto instanceRet = mdns_instance_name_set(serviceName.c_str());
    if (instanceRet != ESP_OK) {
      BELL_LOG(error, LOG_TAG, "mdns_instance_name_set failed: {}",
              (int)instanceRet);
      return nonstd::make_unexpected(
          bell::mdns::MdnsErrc::service_registration_failed);
    }

    auto advertiser = std::make_unique<EspressifMDNAdvertiser>();
    auto res =
        advertiser->advertise(serviceName, serviceType, port, txtRecords);

    if (!res) {
      return nonstd::make_unexpected(res.error());
    }

    return advertiser;
  }

  // Returns a pointer to the singleton instance
  static EspressifMDNSManager* getDefaultManager() {
    static EspressifMDNSManager* defaultManagerInstance = nullptr;

    if (!defaultManagerInstance) {
      defaultManagerInstance = new EspressifMDNSManager();
    }
    return defaultManagerInstance;
  }

 private:
  const char* LOG_TAG = "EspressifMDNSManager";

  // Held weak so the dispatcher task (and its stack) exists only while a
  // browser from browse() below is actually using it.
  std::weak_ptr<BrowseDispatcher> browseDispatcher;
};

Manager* bell::mdns::getDefaultManager() {
  return EspressifMDNSManager::getDefaultManager();
}

#endif
