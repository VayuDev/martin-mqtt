#include "MQTTMessage.hpp"

namespace martin {

MQTTMessage::MQTTMessage(std::string topic, std::string payload, martin::Retain retain, martin::QoS qos)
: mTopic(std::move(topic)), mPayload(std::move(payload)), mRetain(retain), mQoS(qos) {
}

}