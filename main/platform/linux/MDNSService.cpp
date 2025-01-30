#include "bell/mdns/Service.h"

#ifndef BELL_DISABLE_MDNS

#include <cassert>
#include <functional>
#include <vector>

// Library includes
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>
#include <fmt/format.h>

// Own includes
#include "bell/Logger.h"
#include "bell/utils/Semaphore.h"

using namespace bell;

namespace {
const char* LOG_TAG = "AvahiMDNSResponder";
}

class implMDNSService : public mdns::Service {
 public:
  using UnregisterCallback = std::function<void()>;

  // Very dumb implementation of the service, just calls the unregister callback on destruction, which should unregister the service from Avahi.
  implMDNSService(UnregisterCallback callback)
      : unregisterCallback(std::move(callback)) {}
  ~implMDNSService() override { unregisterService(); }

  implMDNSService(const implMDNSService&) = delete;
  implMDNSService& operator=(const implMDNSService&) = delete;

  void unregisterService() override {
    if (unregisterCallback) {
      unregisterCallback();
    }
  }

 private:
  UnregisterCallback unregisterCallback;
};

class AvahiMDNSResponder {
 public:
  AvahiMDNSResponder() : avahiPoll(avahi_simple_poll_new()) {
    if (avahiPoll == nullptr) {
      throw std::runtime_error("Failed to create Avahi poll object");
    }

    // Create the Avahi client
    avahiClient = avahi_client_new(
        avahi_simple_poll_get(avahiPoll), static_cast<AvahiClientFlags>(0),
        avahiClientCallback, &avahiConnectedSemaphore, nullptr);

    if (avahiClient == nullptr) {
      throw std::runtime_error("Failed to create Avahi client");
    }

    // Wait for the Avahi client to connect. Technically most of the cases this should be instant, with the semaphore already given by the callback.
    if (!avahiConnectedSemaphore.take(5000)) {
      throw std::runtime_error("Connection to Avahi client timed out");
    }
  };

  ~AvahiMDNSResponder() { cleanupAvahiState(); }

  AvahiMDNSResponder(const AvahiMDNSResponder&) = delete;
  AvahiMDNSResponder& operator=(const AvahiMDNSResponder&) = delete;

  void cleanupAvahiState() {
    std::scoped_lock lock(groupMutex);
    // Remove all the entry groups
    for (auto& entryGroup : entryGroups) {
      freeEntryGroup(entryGroup);
    }

    entryGroups.clear();

    // Free the Avahi client and poll objects
    if (avahiClient != nullptr) {
      avahi_client_free(avahiClient);
    }

    if (avahiPoll != nullptr) {
      avahi_simple_poll_free(avahiPoll);
    }
  }

  std::unique_ptr<implMDNSService> registerService(
      const std::string& serviceName, const std::string& serviceType,
      const std::string& serviceProto, const std::string& serviceHost,
      int servicePort, const std::map<std::string, std::string>& txtData) {
    std::scoped_lock lock(groupMutex);

    // Create a new Avahi entry group
    AvahiEntryGroup* entryGroup =
        avahi_entry_group_new(avahiClient, avahiGroupCallback, nullptr);

    if (entryGroup == nullptr) {
      throw std::runtime_error("Failed to create Avahi entry group");
    }

    // Construct the TXT data
    AvahiStringList* avahiTxt = nullptr;
    for (const auto& [key, value] : txtData) {
      avahiTxt =
          avahi_string_list_add_pair(avahiTxt, key.c_str(), value.c_str());
    }

    // Register the service with the constructed TXT data
    std::string type = fmt::format("{}.{}", serviceType, serviceProto);
    int ret = avahi_entry_group_add_service_strlst(
        entryGroup, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, (AvahiPublishFlags)0,
        serviceName.c_str(), type.c_str(), nullptr,
        serviceHost.empty() ? nullptr : serviceHost.c_str(), servicePort,
        avahiTxt);

    // Free the TXT data, no longer needed at this point
    avahi_string_list_free(avahiTxt);

    if (ret >= 0) {
      // Success, commit the entry group
      ret = avahi_entry_group_commit(entryGroup);
    } else if (ret < 0) {
      BELL_LOG(error, LOG_TAG, "Failed to register service: {}",
               avahi_strerror(ret));
      avahi_entry_group_free(entryGroup);
      throw std::runtime_error("Failed to register service");
    }

    // Add the entry group to the list
    entryGroups.push_back(entryGroup);

    // Return a service implementation instance, which will unregister the service on destruction
    return std::make_unique<implMDNSService>([this, entryGroup]() {
      std::scoped_lock lock(groupMutex);
      // Destroy the entry group
      freeEntryGroup(entryGroup);

      // Remove the entry group from the registered list
      entryGroups.erase(
          std::remove(entryGroups.begin(), entryGroups.end(), entryGroup),
          entryGroups.end());
    });
  }

  // Singleton instance getter
  static AvahiMDNSResponder* getInstance() {
    static auto instance =
        std::make_unique<AvahiMDNSResponder>();  // Singleton instance
    return instance.get();
  }

 private:
  AvahiSimplePoll* avahiPoll = nullptr;       // Avahi poll object
  AvahiClient* avahiClient = nullptr;         // Avahi client object
  utils::Semaphore avahiConnectedSemaphore;   // Semaphore for Avahi connection
  std::mutex groupMutex;                      // Mutex for Avahi entry groups
  std::vector<AvahiEntryGroup*> entryGroups;  // List of Avahi entry groups

  // Resets and frees an Avahi entry group
  static void freeEntryGroup(AvahiEntryGroup* entryGroup) {
    avahi_entry_group_reset(entryGroup);
    avahi_entry_group_free(entryGroup);
  }

  // Callback for Avahi client state changes, reports the running state, as well as logs any failures
  static void avahiClientCallback(AvahiClient* client, AvahiClientState state,
                                  void* userdata) {
    assert(client != nullptr);
    auto* connectedSem = static_cast<utils::Semaphore*>(userdata);

    switch (state) {
      case AVAHI_CLIENT_S_RUNNING:
        BELL_LOG(debug, LOG_TAG, "Avahi client running");
        connectedSem->give();
        break;
      case AVAHI_CLIENT_FAILURE:
        BELL_LOG(error, LOG_TAG, "Avahi client failure {}",
                 avahi_strerror(avahi_client_errno(client)));
        throw std::runtime_error("Avahi client failure");
        break;
      default:
        BELL_LOG(debug, LOG_TAG, "Avahi client state: {}",
                 static_cast<int>(state));
        break;
    }
  }

  // Callback for Avahi entry group state changes, used for debugging
  static void avahiGroupCallback(AvahiEntryGroup* group,
                                 AvahiEntryGroupState state,
                                 void* /*userdata*/) {
    assert(group != nullptr);
    switch (state) {
      case AVAHI_ENTRY_GROUP_ESTABLISHED:
        BELL_LOG(debug, LOG_TAG, "Avahi entry group established");
        break;
      case AVAHI_ENTRY_GROUP_COLLISION:
        BELL_LOG(debug, LOG_TAG, "Avahi entry group collision");
        break;
      case AVAHI_ENTRY_GROUP_FAILURE:
        BELL_LOG(debug, LOG_TAG, "Avahi entry group failure");
        break;
      case AVAHI_ENTRY_GROUP_UNCOMMITED:
        BELL_LOG(debug, LOG_TAG, "Avahi entry group uncommitted");
        break;
      default:
        BELL_LOG(debug, LOG_TAG, "Avahi entry group state: {}",
                 static_cast<int>(state));
        break;
    }
  }
};

/**
 * Avahi implementation of mdns::Service.
 * @see https://avahi.org/doxygen/html/
 * @remark This implementation does not handle collisions between services.
 **/
std::unique_ptr<mdns::Service> mdns::Service::registerService(
    const std::string& serviceName, const std::string& serviceType,
    const std::string& serviceProto, const std::string& serviceHost,
    int servicePort, const std::map<std::string, std::string>& txtData) {
  return AvahiMDNSResponder::getInstance()->registerService(
      serviceName, serviceType, serviceProto, serviceHost, servicePort,
      txtData);
}

#endif  // BELL_DISABLE_MDNS