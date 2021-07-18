#pragma once
#include <string_view>
#include "Enums.hpp"
#include "Forward.hpp"

namespace martin {

class AbstractMQTTMessageHandler {
public:
    virtual void handleMessage(const MQTTMessage& msg) = 0;
    void operator()(const MQTTMessage& msg) {
        handleMessage(msg);
    }
};

}