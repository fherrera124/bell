#include "bell/mdns/Browser.h"

// System includes
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

// Library includes
#include "bell/net/IpAddress.h"
#include "dns_sd.h"

// Bell includes
#include "bell/Logger.h"
#include "bell/utils/Utils.h"

using namespace bell;

namespace {
// @brief Converts a DNSServiceErrorType to a human-readable string
const char* dnsSdErrorToString(DNSServiceErrorType errorCode) {
  switch (errorCode) {
    case kDNSServiceErr_NoError:
      return "kDNSServiceErr_NoError";
    case kDNSServiceErr_Unknown:
      return "kDNSServiceErr_Unknown";
    case kDNSServiceErr_NoSuchName:
      return "kDNSServiceErr_NoSuchName";
    case kDNSServiceErr_NoMemory:
      return "kDNSServiceErr_NoMemory";
    case kDNSServiceErr_BadParam:
      return "kDNSServiceErr_BadParam";
    case kDNSServiceErr_BadReference:
      return "kDNSServiceErr_BadReference";
    case kDNSServiceErr_BadState:
      return "kDNSServiceErr_BadState";
    case kDNSServiceErr_BadFlags:
      return "kDNSServiceErr_BadFlags";
    case kDNSServiceErr_Unsupported:
      return "kDNSServiceErr_Unsupported";
    case kDNSServiceErr_NotInitialized:
      return "kDNSServiceErr_NotInitialized";
    case kDNSServiceErr_AlreadyRegistered:
      return "kDNSServiceErr_AlreadyRegistered";
    case kDNSServiceErr_NameConflict:
      return "kDNSServiceErr_NameConflict";
    case kDNSServiceErr_Invalid:
      return "kDNSServiceErr_Invalid";
    case kDNSServiceErr_Firewall:
      return "kDNSServiceErr_Firewall";
    case kDNSServiceErr_Incompatible:
      return "kDNSServiceErr_Incompatible";
    case kDNSServiceErr_BadInterfaceIndex:
      return "kDNSServiceErr_BadInterfaceIndex";
    case kDNSServiceErr_Refused:
      return "kDNSServiceErr_Refused";
    case kDNSServiceErr_NoSuchRecord:
      return "kDNSServiceErr_NoSuchRecord";
    case kDNSServiceErr_NoAuth:
      return "kDNSServiceErr_NoAuth";
    case kDNSServiceErr_NoSuchKey:
      return "kDNSServiceErr_NoSuchKey";
    case kDNSServiceErr_NATTraversal:
      return "kDNSServiceErr_NATTraversal";
    case kDNSServiceErr_DoubleNAT:
      return "kDNSServiceErr_DoubleNAT";
    case kDNSServiceErr_BadTime:
      return "kDNSServiceErr_BadTime";
    default:
      return "Unknown";
  }
}
}  // namespace

// MacOS implementation of mdns::Browser, using the Apple Bonjour API
class implMDNSBrowser : public mdns::Browser {
 public:
  implMDNSBrowser(const std::string& regType, const std::string& regDomain,
                  int interfaceIndex, DiscoveryEventCallback onEvent,
                  bool autoResolveService = true,
                  bool autoResolveAddresses = true, bool resolveIpv6 = true)
      : autoResolveService(autoResolveService),
        autoResolveAddresses(autoResolveAddresses),
        resolveIpv6(resolveIpv6),
        onEvent(std::move(onEvent)) {
    // Create the service reference
    DNSServiceErrorType err = DNSServiceCreateConnection(&rootRef);
    if (err != kDNSServiceErr_NoError) {
      throw std::runtime_error(
          fmt::format("Failed to create dns-sd connection, err={}",
                      dnsSdErrorToString(err)));
    }

    // Create copy of the root ref for the browse call
    browseRef = rootRef;

    // Start browsing for services
    err = DNSServiceBrowse(&browseRef, kDNSServiceFlagsShareConnection,
                           interfaceIndex, regType.c_str(),
                           regDomain.empty() ? nullptr : regDomain.c_str(),
                           browseReplyShim, this);
    if (err != kDNSServiceErr_NoError) {
      throw std::runtime_error(
          fmt::format("Failed to start service discovery, errcode={}",
                      dnsSdErrorToString(err)));
    }

    sockFd = DNSServiceRefSockFD(rootRef);
    if (sockFd == -1) {
      throw std::runtime_error("Failed to get socket fd for dns-sd service");
    }
  }

  // Delete copy constructor and copy assignment operator
  implMDNSBrowser(const implMDNSBrowser&) = delete;
  implMDNSBrowser& operator=(const implMDNSBrowser&) = delete;

  ~implMDNSBrowser() override { stopDiscovery(); }

  // Holds current discovered record while resolve and addr resolve are pending, takes care of destructing the service references
  struct CachedMDNSRecord {
    DiscoveredRecord record{};
    DNSServiceRef resolveRef = nullptr;
    DNSServiceRef addrResolveRef = nullptr;
    implMDNSBrowser* browserPtr = nullptr;

    CachedMDNSRecord(uint32_t interfaceIndex, const char* name,
                     const char* regType, const char* regDomain,
                     implMDNSBrowser* browserPtr)
        : browserPtr(browserPtr) {
      record.interfaceIndex = interfaceIndex;
      record.name = name;
      record.regType = regType;
      record.domain = regDomain;
    }

    // Delete copy constructor and copy assignment operator
    CachedMDNSRecord(const CachedMDNSRecord&) = delete;
    CachedMDNSRecord& operator=(const CachedMDNSRecord&) = delete;

    bool compareByRegType(uint32_t interfaceIndex, const char* name,
                          const char* regType, const char* regDomain) const {
      return interfaceIndex == record.interfaceIndex && record.name == name &&
             record.regType == regType && record.domain == regDomain;
    }

    ~CachedMDNSRecord() {
      if (resolveRef != nullptr) {
        DNSServiceRefDeallocate(resolveRef);
      }
    }
  };

  void processEvents(int timeoutMs) override {
    if (rootRef == nullptr) {
      throw std::runtime_error("Service ref not initialized");
    }

    // Select and handle data on discovery socket
    FD_ZERO(&readFds);
    FD_SET(sockFd, &readFds);

    auto tv =
        bell::utils::millisecondsToTimeval(timeoutMs);  // Create posix timeout
    int result = ::select(sockFd + 1, &readFds, nullptr, nullptr, &tv);
    if (result > 0) {
      if (FD_ISSET(sockFd, &readFds)) {
        // Got data on the socket
        auto err = DNSServiceProcessResult(rootRef);

        if (err != kDNSServiceErr_NoError) {
          throw std::runtime_error(
              fmt::format("Failed to process dns-sd results, errcode={}",
                          dnsSdErrorToString(err)));
        }
      }
    } else if (result < 0) {
      if (errno != EINTR)
        throw std::runtime_error(fmt::format(
            "mdns Discovery ::select failed, err={}", strerror(errno)));
    }
  }

  void stopDiscovery() override {
    cachedRecords
        .clear();  // Will invalidate all pending resolve / addr resolve references

    // Invalidate the root reference
    DNSServiceRefDeallocate(rootRef);

    browseRef = nullptr;
    rootRef = nullptr;
  }

  void resolveService(const DiscoveredRecord& service) override {
    if (service.serviceResolved) {
      throw std::runtime_error("Service already resolved");
    }

    for (auto& recordPtr : cachedRecords) {
      if (recordPtr->record == service) {
        recordPtr->resolveRef = rootRef;  // Copy the root reference
        auto err = DNSServiceResolve(
            &recordPtr->resolveRef, kDNSServiceFlagsShareConnection,
            recordPtr->record.interfaceIndex, recordPtr->record.name.c_str(),
            recordPtr->record.regType.c_str(), recordPtr->record.domain.c_str(),
            resolveReplyShim, recordPtr.get());

        if (err != kDNSServiceErr_NoError) {
          BELL_LOG(error, LOG_TAG,
                   "Failed to resolve service. Name={}, errcode={}",
                   recordPtr->record.name, dnsSdErrorToString(err));
          onEvent(mdns::EventType::ServiceResolveFailed, recordPtr->record);
        }

        break;  // should only have one matching record
      }
    }
  }

  void resolveAddress(const DiscoveredRecord& service) override {
    if (!service.serviceResolved) {
      throw std::runtime_error("Service needs to be resolved first");
    }

    for (auto& recordPtr : cachedRecords) {
      if (recordPtr->record == service) {
        recordPtr->addrResolveRef = rootRef;  // Copy the root reference
        auto err = DNSServiceGetAddrInfo(
            &recordPtr->addrResolveRef, kDNSServiceFlagsShareConnection,
            service.interfaceIndex,
            resolveIpv6 ? kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6
                        : kDNSServiceProtocol_IPv4,
            recordPtr->record.hostname.c_str(), &getAddrInfoReplyShim,
            recordPtr.get());

        if (err != kDNSServiceErr_NoError) {
          BELL_LOG(error, LOG_TAG,
                   "Failed to resolve service's address. Name={}, errcode={}",
                   recordPtr->record.name, dnsSdErrorToString(err));
          onEvent(mdns::EventType::AddressResolveFailed, recordPtr->record);
        }

        break;  // should only have one matching record
      }
    }
  }

  void getAddrInfoReply(CachedMDNSRecord* recordPtr, DNSServiceFlags flags,
                        uint32_t interfaceIndex, DNSServiceErrorType errorCode,
                        const char* /*hostname*/,
                        const struct sockaddr* address, uint32_t /*ttl*/) {
    assert(interfaceIndex == recordPtr->record.interfaceIndex);

    if (errorCode != kDNSServiceErr_NoError) {
      BELL_LOG(error, LOG_TAG,
               "Failed to resolve service's address. Name={}, errcode={}",
               recordPtr->record.name, dnsSdErrorToString(errorCode));
      onEvent(mdns::EventType::AddressResolveFailed, recordPtr->record);
      return;
    }

    // Successfuly resolved the service
    recordPtr->record.addresses.emplace_back(address);

    if (!(flags & kDNSServiceFlagsMoreComing)) {
      // Successfuly resolved service's address
      onEvent(mdns::EventType::AddressResolved, recordPtr->record);
    }
  }

  void resolveReply(CachedMDNSRecord* recordPtr, DNSServiceFlags /*flags*/,
                    uint32_t interfaceIndex, DNSServiceErrorType errorCode,
                    const char* fullname, const char* hostname,
                    uint16_t port, /* In network byte order */
                    uint16_t txtLen, const unsigned char* txtRecord) {
    assert(interfaceIndex == recordPtr->record.interfaceIndex);

    if (errorCode != kDNSServiceErr_NoError) {
      BELL_LOG(error, LOG_TAG, "Failed to resolve service. Name={}, errcode={}",
               recordPtr->record.name, dnsSdErrorToString(errorCode));
      onEvent(mdns::EventType::ServiceResolveFailed, recordPtr->record);
      return;
    }

    // Fill out the received information
    recordPtr->record.hostname = hostname;
    recordPtr->record.fullName = fullname;
    recordPtr->record.port = ntohs(port);
    recordPtr->record.serviceResolved = true;

    try {
      recordPtr->record.parseTXTRecords(txtRecord, txtLen);
    } catch (const std::exception& err) {
      BELL_LOG(error, LOG_TAG,
               "Failed to parse TXT records for service {}. Error: {}",
               recordPtr->record.name, err.what());
      onEvent(mdns::EventType::ServiceResolveFailed, recordPtr->record);
      return;
    }

    // Successfuly resolved the service
    onEvent(mdns::EventType::ServiceResolved, recordPtr->record);

    if (autoResolveAddresses) {
      resolveAddress(recordPtr->record);
    }
  }

  void browseReply(DNSServiceFlags flags, uint32_t interfaceIndex,
                   DNSServiceErrorType errorCode, const char* serviceName,
                   const char* regType, const char* replyDomain) {
    if (errorCode != kDNSServiceErr_NoError) {
      throw std::runtime_error(
          fmt::format("Failed to browse for services, errcode={}",
                      dnsSdErrorToString(errorCode)));
    }

    // Try to find the service in the list
    auto matchingRecord =
        std::find_if(cachedRecords.begin(), cachedRecords.end(),
                     [&serviceName, &regType, &replyDomain,
                      interfaceIndex](const auto& record) {
                       return record->compareByRegType(
                           interfaceIndex, serviceName, regType, replyDomain);
                     });

    if (flags & kDNSServiceFlagsAdd) {
      if (matchingRecord == cachedRecords.end()) {
        // Create a new record
        cachedRecords.emplace_back(std::make_unique<CachedMDNSRecord>(
            interfaceIndex, serviceName, regType, replyDomain, this));
        matchingRecord = cachedRecords.end() - 1;
      }

      // Invoke the event callback
      onEvent(mdns::EventType::ServiceAdded, (*matchingRecord)->record);

      if (autoResolveService) {
        // Resolve the service
        resolveService((*matchingRecord)->record);
      }
    } else if (matchingRecord != cachedRecords.end()) {
      onEvent(mdns::EventType::ServiceRemoved, (*matchingRecord)->record);

      // Remove record from the list, this will stop all pending resolves and addr resolves
      cachedRecords.erase(matchingRecord);
    }
  }

  // Thin shim passing dns-sd C-style address info callback to member function
  static void DNSSD_API getAddrInfoReplyShim(
      DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex,
      DNSServiceErrorType errorCode, const char* hostname,
      const struct sockaddr* address, uint32_t ttl, void* ctx) {
    auto* cachedRecord = static_cast<CachedMDNSRecord*>(ctx);

    if (cachedRecord != nullptr && cachedRecord->browserPtr != nullptr) {
      cachedRecord->browserPtr->getAddrInfoReply(cachedRecord, flags,
                                                 interfaceIndex, errorCode,
                                                 hostname, address, ttl);
    } else {
      // Invalidate the reference
      DNSServiceRefDeallocate(ref);
    }
  }

  // Thin shim passing dns-sd C-style resolve callback to member function
  static void DNSSD_API resolveReplyShim(
      DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex,
      DNSServiceErrorType errorCode, const char* fullname, const char* hostname,
      uint16_t port, /* In network byte order */
      uint16_t txtLen, const unsigned char* txtRecord, void* ctx) {
    auto* cachedRecord = static_cast<CachedMDNSRecord*>(ctx);

    if (cachedRecord != nullptr && cachedRecord->browserPtr != nullptr) {
      cachedRecord->browserPtr->resolveReply(
          cachedRecord, flags, interfaceIndex, errorCode, fullname, hostname,
          port, txtLen, txtRecord);
    } else {
      // Invalidate the reference
      DNSServiceRefDeallocate(ref);
    }
  }

  // Thin shim passing C-style browse callback to member function
  static void DNSSD_API browseReplyShim(
      DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex,
      DNSServiceErrorType errorCode, const char* serviceName,
      const char* regType, const char* replyDomain, void* ctx) {
    auto* browser = static_cast<implMDNSBrowser*>(ctx);
    if (browser != nullptr) {
      browser->browseReply(flags, interfaceIndex, errorCode, serviceName,
                           regType, replyDomain);
    } else {
      // invalidate the service reference
      DNSServiceRefDeallocate(ref);
    }
  }

 private:
  const char* LOG_TAG = "AppleMDNSBrowser";
  bool autoResolveService;
  bool autoResolveAddresses;
  bool resolveIpv6;
  DiscoveryEventCallback onEvent;

  // dns-sd browser reference
  DNSServiceRef rootRef = nullptr;
  DNSServiceRef browseRef = nullptr;

  // dns-sd sockfd used for event processing
  dnssd_sock_t sockFd = -1;

  // Used for ::select
  fd_set readFds{};

  std::vector<std::unique_ptr<CachedMDNSRecord>> cachedRecords;
};

/**
 * MacOS implementation of mdns::Browser.
 * @see https://developer.apple.com/documentation/dnssd/1804733-dnsserviceregister
 **/
std::unique_ptr<mdns::Browser> mdns::Browser::startDiscovery(
    const std::string& regType, const std::string& regDomain,
    int interfaceIndex, const DiscoveryEventCallback& onEvent,
    bool autoResolveService, bool autoResolveAddresses, bool resolveIpv6) {
  return std::make_unique<implMDNSBrowser>(regType, regDomain, interfaceIndex,
                                           onEvent, autoResolveService,
                                           autoResolveAddresses, resolveIpv6);
}
