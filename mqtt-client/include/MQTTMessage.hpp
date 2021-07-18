#pragma once

#include <string>
#include <variant>
#include <vector>

#include "Enums.hpp"

namespace martin {

class MQTTMessage {
private:
    std::string mTopic;
    std::string mPayload;
    Retain mRetain;
    QoS mQoS;

public:
    MQTTMessage(std::string topic, std::string payload, Retain retain = Retain::No, QoS qos = QoS::QoS0);
    const std::string& getTopic() const {
        return mTopic;
    }
    const auto& getPayload() const {
        return mPayload;
    }
    auto isRetained() const {
        return mRetain == Retain::Yes;
    }
    auto getQoS() const {
        return mQoS;
    }
};

}
