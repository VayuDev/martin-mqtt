#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "Forward.hpp"
#include "MQTTMessage.hpp"
#include <functional>

namespace martin {

/**
 * This class represents a configuration for a MQTTClient.
 * Please use the MQTTConfigBuilder class to create an instance.
 */
class MQTTConfig final {
private:
    std::optional<MQTTMessage> mWillMsg;
    std::string mHostname;
    uint16_t mPort = 1883;
    std::string mUsername;
    std::string mPassword;
    std::string mClientID;
    CleanSession mCleanSession = CleanSession::Yes;
    uint16_t mKeepAliveIntervalSeconds = 20;
    int32_t mMaxRetries = -1;
    std::chrono::milliseconds mDelayBetweenRetries = std::chrono::milliseconds(500);
    std::function<void()> mOnConnectCallback;
    friend MQTTConfigBuilder;

public:
    [[nodiscard]] const auto& getWillMsg() const {
        return mWillMsg;
    }
    [[nodiscard]] const auto& getHostname() const {
        return mHostname;
    }
    [[nodiscard]] auto getPort() const {
        return mPort;
    }
    [[nodiscard]] const auto& getUsername() const {
        return mUsername;
    }
    [[nodiscard]] const auto& getPassword() const {
        return mPassword;
    }
    [[nodiscard]] auto getClientID() const {
        return mClientID;
    }
    [[nodiscard]] bool isCleanSession() const {
        return mCleanSession == CleanSession::Yes;
    }
    [[nodiscard]] auto getKeepAliveIntervalSeconds() const {
        return mKeepAliveIntervalSeconds;
    }
    [[nodiscard]] auto getMaxRetries() const {
        return mMaxRetries;
    }
    [[nodiscard]] auto getDelayBetweenRetries() const {
        return mDelayBetweenRetries;
    }
    [[nodiscard]] const auto& getOnConnectCallback() const {
        return mOnConnectCallback;
    }
};

}
