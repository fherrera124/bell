#pragma once

#ifndef BELL_DISABLE_MQTT

// Standard includes
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// Library includes
#include "mqtt.h"

// Bell includes
#include "bell/net/Socket.h"

namespace bell::net {
/// MQTTClient is a thin wrapper around the MQTT client library.
class MQTTClient {
 public:
  MQTTClient() = default;
  ~MQTTClient() = default;

  enum class QOS {
    AT_MOST_ONCE = MQTT_PUBLISH_QOS_0,
    AT_LEAST_ONCE = MQTT_PUBLISH_QOS_1,
    EXACTLY_ONCE = MQTT_PUBLISH_QOS_2
  };

  /**
   * @brief Callback for when a message is published.
   * @param topic The topic the message was published to.
   * @param message The message that was published.
   */
  using PublishCallback =
      std::function<void(const std::string&, const std::string&)>;

  /**
   * @brief Set the catch-all publish callback
   * @param callback The callback to set.
   */
  void setPublishCallback(const PublishCallback& callback);

  /**
   * @brief Connect to an MQTT broker.
   *
   * @param host The host to connect to.
   * @param port The port to connect to.
   * @param username The username to authenticate with.
   * @param password The password to authenticate with.
   * @param timeoutMs The timeout for the connection, in milliseconds.
   */
  void connect(const std::string& host, uint16_t port,
               const std::string& username = "",
               const std::string& password = "", int timeoutMs = 0);

  /**
   * @brief Disconnect from the MQTT broker.
   */
  void disconnect();

  /**
   * @brief Synchronize with the MQTT broker.
   */
  void sync();

  /**
   * @brief Publish a message to a topic.
   *
   * @param topic The topic to publish to.
   * @param message The message to publish.
   * @param qos The quality of service to publish with.
   */
  void publish(const std::string& topic, const std::string& message,
               QOS qos = QOS::AT_MOST_ONCE);

  /**
   * @brief Subscribe to a topic.
   *
   * @param topic The topic to subscribe to.
   * @param qos The quality of service to subscribe with.
   */
  void subscribe(const std::string& topic, QOS qos = QOS::AT_MOST_ONCE);

  /**
   * @brief Unsubscribe from a topic.
   *
   * @param topic The topic to unsubscribe from.
   */
  void unsubscribe(const std::string& topic);

  /**
   * @brief Check if the MQTT client is connected.
   *
   * @return True if the MQTT client is connected, false otherwise.
   */
  bool isConnected() const;

  // Directly mapped from mqtt client's publish_callback field
  void onPublishCallback(struct mqtt_response_publish* published);

 private:
  std::unique_ptr<net::Socket> socket;
  std::atomic<bool> connected = false;
  PublishCallback publishCallback;

  // mqtt lib internals
  struct mqtt_client client {};
  std::array<uint8_t, 2048> sendbuf{};
  std::array<uint8_t, 1024> recvbuf{};
};
}  // namespace bell::net

#endif  // BELL_DISABLE_MQTT