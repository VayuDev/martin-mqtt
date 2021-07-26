#pragma once

#include <chrono>
#include <optional>

#include "Enums.hpp"
#include "MQTTConfig.hpp"
#include "MQTTMessage.hpp"
#include <functional>

namespace martin {

class MQTTConfigBuilder {
private:
    MQTTConfig mConfig;

public:
    /**
     * Sets the will message. This message will be sent if the connection is closed without calling
     * MQTTClient::disconnect first. Can be left empty.
     *
     * @param willMsg The will message.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& will(MQTTMessage willMsg);

    /**
     * Sets the hostname of the MQTT broker. Can be an IP-address or a proper hostname.
     *
     * @param hostname The new hostname.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& hostname(std::string hostname);

    /**
     * Sets the remote port of the MQTT broker.
     *
     * @param port The new remote port.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& port(uint16_t port);

    /**
     * Sets the username for connecting to the MQTT broker. Can be left empty.
     *
     * @param username The new username.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& username(std::string username);

    /**
     * Sets the password for connecting to the MQTT broker. Can be left empty.
     *
     * @param password The new password.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& password(std::string password);

    /**
     * Sets the Client ID for connecting to the MQTT broker. This can be used for persistent
     * sessions or be left empty. Some MQTT brokers don't support empty client ids, so beware.
     *
     * @param clientID The new clientID.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& clientID(std::string clientID);

    /**
     * Sets the cleanSession flag. Default is CleanSession::Yes.
     *
     * @param cleanSession The new cleanSession flag.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& cleanSession(CleanSession cleanSession);

    /**
     * Sets the keep alive interval in seconds. Default is 20. Don't set this value to high.
     *
     * @param keepAliveInterval The new keep alive interval value in seconds.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& keepAliveInterval(uint16_t keepAliveInterval);

    /**
     * The delay between retries if the connection is dropped. Default is 500ms.
     *
     * @param delayBetweenRetries The delay between connection retries.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& delayBetweenRetries(std::chrono::milliseconds delayBetweenRetries);

    /**
     * The maximum amount of retries until MQTTClient::loop will throw an exception. -1 is equal to
     * infinity. Default is -1.
     *
     * @param retries The maximum amount of retries to connect.
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& maxRetriesOnDisconnect(int retries);

    /**
     * Will be called whenever the client (re-)connects to the broker.
     *
     * @param onConnectCallback The function to call
     * @return A reference to this Object.
     */
    MQTTConfigBuilder& onConnect(std::function<void()> onConnectCallback);

    /**
     * Returns a copy of the generated MQTTConfig.
     *
     * @return The generated MQTTConfig.
     */
    [[nodiscard]] MQTTConfig build() const;
};
}