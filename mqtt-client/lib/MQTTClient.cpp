#include "MQTTClient.hpp"
#include "Helper.hpp"
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace martin {

MQTTClient::MQTTClient(MQTTConfig config) : mConfig(std::move(config)) {
    mLastReconnect = std::chrono::steady_clock::now() - mConfig.getDelayBetweenRetries() * 2;
    connectMqtt();
}

void MQTTClient::enqueueSendFront(OnDisconnect shouldKeep, std::vector<uint8_t> buffer, std::function<CallbackStatus()> onDone) {
    std::lock_guard<std::recursive_mutex> guard{ mMutex };
    mTaskQueue.emplace_front(std::make_pair(shouldKeep, [this, shouldKeep, buffer = std::move(buffer), onDone = std::move(onDone)]() mutable {
        auto sentBytes = mClient->send(buffer.data(), buffer.size());
        std::vector<uint8_t> newBuffer(buffer.begin() + sentBytes, buffer.end());
        if(!newBuffer.empty()) {
            enqueueSendFront(OnDisconnect::DELETE_ME, std::move(newBuffer), std::move(onDone));
            return CallbackStatus::OK;
        }
        auto onDoneRet = onDone();
        if(onDoneRet == CallbackStatus::REPEAT_ME) {
            mTaskQueue.emplace_front(std::make_pair(shouldKeep, std::move(onDone)));
            return CallbackStatus::OK;
        }
        return onDoneRet;
    }));
}

void MQTTClient::publish(MQTTMessage message) {
    // only QoS0 supported right now
    assert(message.getQoS() == QoS::QoS0);

    std::lock_guard<std::recursive_mutex> guard{ mMutex };
    if(mDisconnected)
        throw std::runtime_error{ "Already disconnected!" };
    mTaskQueue.emplace_back(std::make_pair(OnDisconnect::KEEP_ME, [this, message = std::move(message)]() -> CallbackStatus {
        std::vector<uint8_t> buffer;
        uint8_t firstByte = static_cast<uint8_t>(MessageType::PUBLISH) << 4;
        // skip dup flag because we only need it in QoS >= 1 which we don't support
        firstByte |= (static_cast<uint8_t>(message.getQoS()) << 1);
        if(message.isRetained()) {
            firstByte |= 1;
        }
        buffer.emplace_back(firstByte);
        appendString2ByteLengthToBuffer(buffer, message.getTopic());
        // skip packet identifier because we only need it in QoS >= 1 which we don't support
        appendToBuffer(buffer, message.getPayload());
        insertMessageLengthToHeader(buffer);
        enqueueSendFront(OnDisconnect::KEEP_ME, buffer, []() -> CallbackStatus {
            // PUBACK only gets send in QoS >= 1
            return CallbackStatus::OK;
        });
        return CallbackStatus::OK;
    }));
}

void MQTTClient::subscribe(std::string topic, QoS qos, MQTTPublishCallback callback) {
    std::lock_guard<std::recursive_mutex> guard{ mMutex };
    if(mDisconnected)
        throw std::runtime_error{ "Already disconnected!" };
    mTaskQueue.emplace_back(
        std::make_pair(OnDisconnect::KEEP_ME, [this, topic = std::move(topic), qos, callback = std::move(callback)]() mutable -> CallbackStatus {
            // prepare packet
            std::vector<uint8_t> buffer;
            uint8_t firstByte = static_cast<uint8_t>(MessageType::SUBSCRIBE) << 4;
            // 7th bit is reserved and has to be a 1
            firstByte |= 0b10;
            buffer.emplace_back(firstByte);
            auto id = mPacketIdentifierCounter++;
            appendUint16_tToBuffer(buffer, id);
            appendString2ByteLengthToBuffer(buffer, topic);
            buffer.emplace_back(static_cast<uint8_t>(qos));
            insertMessageLengthToHeader(buffer);

            if(isTopicAdvanced(topic)) {
                mAdvancedCallbacks.emplace_back(std::make_pair(topic, std::move(callback)));
            } else {
                mSimpleCallbacks.emplace(topic, std::move(callback));
            }

            // send packet
            enqueueSendFront(OnDisconnect::DELETE_ME, buffer, [this, id, topic]() -> CallbackStatus {
                // receive SUBACK
                enqueueRecvPacketFront(
                    OnDisconnect::DELETE_ME, [this, id, topic](MessageType type, const std::vector<uint8_t>& bytes) -> CallbackStatus {
                        // check response
                        if(type != MessageType::SUBACK) {
                            protocolViolation();
                            return CallbackStatus::PROTOCOL_VIOLATION;
                        }
                        uint16_t receivedId;
                        memcpy(&receivedId, bytes.data() + 1, 2);
                        receivedId = ntohs(receivedId);
                        if(receivedId != id) {
                            protocolViolation();
                            return CallbackStatus::PROTOCOL_VIOLATION;
                        }
                        uint8_t status;
                        memcpy(&status, bytes.data() + 3, 1);
                        if(status == 0x80) {
                            std::cerr << "SUBSCRIBE for topic '" << topic
                                      << "' failed! The serve rejected this topic, maybe it's "
                                         "invalid?";
                        }
                        return CallbackStatus::OK;
                    });

                return CallbackStatus::OK;
            });
            return CallbackStatus::OK;
        }));
}

void MQTTClient::unsubscribe(std::string topic) {
    std::lock_guard<std::recursive_mutex> guard{ mMutex };
    if(mDisconnected)
        throw std::runtime_error{ "Already disconnected!" };

    // remove callback handlers
    mSimpleCallbacks.erase(topic);
    for(auto it = mAdvancedCallbacks.begin(); it != mAdvancedCallbacks.end();) {
        if(it->first == topic) {
            it = mAdvancedCallbacks.erase(it);
        } else {
            it++;
        }
    }
    mTaskQueue.emplace_back(std::make_pair(OnDisconnect::DELETE_ME, [this, topic = std::move(topic)]() mutable -> CallbackStatus {
        // prepare paket
        std::vector<uint8_t> buffer;
        uint8_t firstByte = static_cast<uint8_t>(MessageType::UNSUBSCRIBE) << 4;
        // reserved, the 7th bit always has to be a one
        firstByte |= 0b10;
        buffer.push_back(firstByte);
        auto id = mPacketIdentifierCounter++;
        appendUint16_tToBuffer(buffer, id);
        appendString2ByteLengthToBuffer(buffer, topic);
        insertMessageLengthToHeader(buffer);
        enqueueSendFront(OnDisconnect::DELETE_ME, buffer, [this, id]() {
            // receive UNSUBACK
            enqueueRecvPacketFront(OnDisconnect::DELETE_ME, [this, id](MessageType type, const std::vector<uint8_t>& buffer) {
                // check response
                if(type != MessageType::UNSUBACK) {
                    protocolViolation();
                    return CallbackStatus::PROTOCOL_VIOLATION;
                }
                uint16_t receivedId;
                memcpy(&receivedId, buffer.data() + 1, 2);
                if(receivedId != id) {
                    protocolViolation();
                    return CallbackStatus::PROTOCOL_VIOLATION;
                }
                return CallbackStatus::OK;
            });
            return CallbackStatus::OK;
        });
        return CallbackStatus::OK;
    }));
}

MQTTClientLoopStatus MQTTClient::loop(std::optional<std::chrono::milliseconds> timeout, BlockForRecv useSelect) {
    std::lock_guard<std::recursive_mutex> guard{ mMutex };

    auto start = std::chrono::steady_clock::now();
    auto blockUntilThereIsSomethingToDo = [&] {
        auto timeAlreadyPassed = std::chrono::steady_clock::now() - start;
        if(timeout.has_value() && timeout.value() <= timeAlreadyPassed) {
            return;
        }
        if(mClient && mClient->getConnectState() == TCPConnectState::CONNECTED) {
            if(useSelect == BlockForRecv::Yes && mTaskQueue.empty()) {
                // In this mode, loop tries to block for at least timeout milliseconds. Very efficient
                auto durationToNextPing = std::chrono::milliseconds((mConfig.getKeepAliveIntervalSeconds() - mClient->getSecondsSinceLastSend()) * 1000);
                std::chrono::milliseconds blockTimeout;
                // ensure that we never block longer than durationToNextPing
                if(timeout.has_value()) {
                    blockTimeout = std::min(durationToNextPing, std::chrono::duration_cast<std::chrono::milliseconds>(timeout.value() - timeAlreadyPassed));
                } else {
                    blockTimeout = durationToNextPing;
                }
                auto blockReturnReason = mClient->blockUntilDataAvailable(blockTimeout);
                if(blockReturnReason == TcpBlockUntilDataAvailableReturnReason::DATA_AVAILABLE) {
                    // Either we received a disconnect or some actual data, so let's act accordingly
                    if(mClient->isDataAvailable() && !mCurrentlyReceivingPacket) {
                        enqueueRecvPacketFront(OnDisconnect::DELETE_ME, {});
                    } else {
                        throw std::runtime_error{ "Disconnected!" };
                    }
                }
            }
        }
    };

    try {


        blockUntilThereIsSomethingToDo();

        while(!mTaskQueue.empty()) {
            // do a single task
            auto currentTask = mTaskQueue.begin();
            auto callbackStatus = currentTask->second();
            if(callbackStatus == CallbackStatus::PROTOCOL_VIOLATION) {
                protocolViolation();
                throw std::runtime_error{ "Server violated the MQTT protocol!" };
            } else if(callbackStatus == CallbackStatus::REPEAT_ME) {
                auto temp = std::move(*currentTask);
                mTaskQueue.erase(currentTask);
                mTaskQueue.emplace_front(std::move(temp));
            } else if(callbackStatus == CallbackStatus::CLEAR_TASK_QUEUE) {
                mTaskQueue.clear();
                return MQTTClientLoopStatus::EVERYTHING_DONE;
            } else if(callbackStatus == CallbackStatus::OK) {
                mTaskQueue.erase(currentTask);
            }

            auto end = std::chrono::steady_clock::now();
            if(timeout && (end - start) >= *timeout) {
                // std::cout << "Tasks done: " << tasksDone << "\n";
                return MQTTClientLoopStatus::TIMEOUT;
            }

            blockUntilThereIsSomethingToDo();
            if(mClient) {
                // idle operations; used for sending pings, enqueuing receives and waiting if there's nothing to do
                if(mClient->isDataAvailable() && !mCurrentlyReceivingPacket) {
                    // server is sending something we're not expecting, it's probably a PUBLISH, so we need
                    // to receive it Those packets get handled automatically, so we don't need a handler
                    enqueueRecvPacketFront(OnDisconnect::DELETE_ME, {});
                }
                if(mClient->getSecondsSinceLastSend() >= mConfig.getKeepAliveIntervalSeconds() && !mPingIsScheduled) {
                    // we have KeepAliveInterval * 1.5 seconds to send the ping, so we don't need to hurry
                    enqueueSendPingReq();
                }
            }
        }

        // std::cout << "Tasks done: " << tasksDone << "\n";
    } catch(std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << "\n";
        if(mDisconnected) {
            return MQTTClientLoopStatus::EXCEPTION_AFTER_DISCONNECT;
        }
        // Some TCP action failed. Reconnect tcp.
        // Only do something after

        mLastReconnect = std::chrono::steady_clock::now();
        if(mTries < mConfig.getMaxRetries() || mConfig.getMaxRetries() < 0) {
            connectMqtt();
        } else {
            throw std::runtime_error("Timeout limit reached");
        }
    }
    return MQTTClientLoopStatus::EVERYTHING_DONE;
}

size_t MQTTClient::getQueuedTaskCount() {
    std::lock_guard<std::recursive_mutex> guard{ mMutex };
    return mTaskQueue.size();
}

void MQTTClient::protocolViolation() {
    mClient.reset();
}

MQTTClient& MQTTClient::operator<<(MQTTMessage msg) {
    publish(std::move(msg));
    return *this;
}

void MQTTClient::enqueueRecvPacketFront(OnDisconnect shouldKeep, std::function<CallbackStatus(MessageType, const std::vector<uint8_t>&)> onDone) {
    mCurrentlyReceivingPacket = true;
    mTaskQueue.emplace_front(std::make_pair(shouldKeep, [this, shouldKeep = shouldKeep, onDone = std::move(onDone)]() mutable -> CallbackStatus {
        // receive packet type
        uint8_t firstByte;
        if(mClient->recv(&firstByte, 1) <= 0) {
            return CallbackStatus::REPEAT_ME;
        }
        // These variables get copied into the lambda and store the receive-state in there. This
        // works because the lambda has the mutable-keyword, allowing it to mutate it's captured
        // state.
        uint32_t multiplier = 1;
        uint32_t packetLength = 0;
        mTaskQueue.emplace_front(std::make_pair(
            shouldKeep, [this, firstByte, shouldKeep = shouldKeep, onDone = std::move(onDone), multiplier, packetLength]() mutable -> CallbackStatus {
                // receive packet length (variable length encoding)
                uint8_t encodedByte;
                do {
                    if(mClient->recv(&encodedByte, 1) <= 0) {
                        return CallbackStatus::REPEAT_ME;
                    }
                    packetLength += (encodedByte & 127) * multiplier;
                    multiplier *= 128;
                    if(multiplier > 128 * 128 * 128) {
                        throw std::runtime_error{ "Malformed remaining length" };
                    }
                } while((encodedByte & 128) != 0);
                // receive buffer
                uint32_t offset = 1;
                std::vector<uint8_t> buffer(packetLength + 1);
                buffer.at(0) = firstByte;
                mTaskQueue.emplace_front(std::make_pair(
                    shouldKeep,
                    [this, firstByte, shouldKeep = shouldKeep, onDone = std::move(onDone), offset, packetLength, buffer]() mutable -> CallbackStatus {
                        if(packetLength > 0) {
                            auto bytesReceived = mClient->recv(buffer.data() + offset, packetLength);
                            offset += bytesReceived;
                            packetLength -= bytesReceived;
                            return CallbackStatus::REPEAT_ME;
                        }
                        // all data received
                        mCurrentlyReceivingPacket = false;
                        auto messageType = static_cast<MessageType>(firstByte >> 4);
                        if(messageType == MessageType::PUBLISH || messageType == MessageType::PUBREL) {
                            handlePublishReceived(messageType, buffer);
                            if(onDone) {
                                // we didn't receive the expected packet, so we try again!
                                enqueueRecvPacketFront(shouldKeep, std::move(onDone));
                            }
                            return CallbackStatus::OK;
                        }
                        if(onDone) {
                            mTaskQueue.emplace_front(
                                    std::make_pair(shouldKeep, [messageType, buffer = std::move(buffer), onDone = std::move(onDone)]() -> CallbackStatus {
                                        return onDone(messageType, buffer);
                                    }));
                        }
                        return CallbackStatus::OK;
                    }));
                return CallbackStatus::OK;
            }));
        return CallbackStatus::OK;
    }));
}

void MQTTClient::handlePublishReceived(MessageType messageType, const std::vector<uint8_t>& buffer) {
    assert(messageType == MessageType::PUBLISH);
    auto qos = static_cast<QoS>((buffer.at(0) & 0b110) >> 1);
    auto retain = static_cast<Retain>(buffer.at(0) == 1);
    assert(qos == QoS::QoS0);
    size_t offset = 1;
    auto topicName = extractStringFromBuffer(buffer, offset);
    // packet identifier only needed for QoS >= 1
    size_t payloadLength = buffer.size() - offset;
    std::string payload;
    payload.reserve(payloadLength);
    for(size_t i = offset; i < buffer.size(); ++i) {
        payload.push_back(buffer.at(i));
    }
    MQTTMessage message{ topicName, std::move(payload), retain, qos };
    for(auto& callback : mAdvancedCallbacks) {
        if(doesTopicMatchPattern(topicName, callback.first)) {
            callback.second(message);
        }
    }
    auto [start, end] = mSimpleCallbacks.equal_range(topicName);
    for(auto it = start; it != end; ++it) {
        it->second(message);
    }
}

void MQTTClient::enqueueSendPingReq() {
    std::lock_guard<std::recursive_mutex> guard{ mMutex };
    mPingIsScheduled = true;
    std::cout << "\nEnqueued ping" << std::endl;
    mTaskQueue.emplace_back(std::make_pair(OnDisconnect::DELETE_ME, [this] {
        // create the packet
        std::vector<uint8_t> buffer(2);
        buffer.at(0) = static_cast<uint8_t>(MessageType::PINGREQ) << 4;
        buffer.at(1) = 0;
        // send PINGREQ
        enqueueSendFront(OnDisconnect::DELETE_ME, buffer, [this]() {
            // sent PINGREQ, now enqueue receive PINGREQ
            std::cout << "\nSent ping" << std::endl;
            mPingIsScheduled = false;
            enqueueRecvPacketFront(OnDisconnect::DELETE_ME, [this](MessageType messageType, const std::vector<uint8_t>& buffer) {
                // received PINGRESP
                std::cout << "Received ping\n";
                if(messageType != MessageType::PINGRESP) {
                    protocolViolation();
                    return CallbackStatus::PROTOCOL_VIOLATION;
                }
                return CallbackStatus::OK;
            });
            return CallbackStatus::OK;
        });
        return CallbackStatus::OK;
    }));
}

void MQTTClient::connectMqtt() {
    std::lock_guard<std::recursive_mutex> guard{ mMutex };

    /* This function contains the whole logic for (re-) connecting to the broker.
     * As tasks are enqueued to the front, you have to read this function front to back
     * to see it in the correct execution order.
     */

    // first, remove all DELETE_ME tasks
    for(auto it = mTaskQueue.begin(); it != mTaskQueue.end();) {
        if(it->first == OnDisconnect::DELETE_ME) {
            it = mTaskQueue.erase(it);
        } else {
            it++;
        }
    }

    // the resubscribe (tasks are put at the end)
    auto advancedCallbacks = std::move(mAdvancedCallbacks);
    mAdvancedCallbacks = {};
    for(auto& elem : advancedCallbacks) {
        // FIXME(Jan): Add QoS into mCallbacks
        subscribe(std::move(elem.first), QoS::QoS0, std::move(elem.second));
    }

    auto simpleCallback = std::move(mSimpleCallbacks);
    mSimpleCallbacks = {};
    for(auto& elem : simpleCallback) {
        // FIXME(Jan): Add QoS into mCallbacks
        subscribe(std::move(elem.first), QoS::QoS0, std::move(elem.second));
    }

    // enqueue the mqtt-CONNECT task to the front
    mTaskQueue.emplace_front(std::make_pair(OnDisconnect::DELETE_ME, [this]() -> CallbackStatus {
        // build the packet
        std::vector<uint8_t> buffer;
        buffer.emplace_back(static_cast<uint8_t>(MessageType::CONNECT) << 4);
        // later insert length here

        // protocol name length
        buffer.emplace_back(0);
        buffer.emplace_back(0x4);
        // protocol identifier
        appendToBuffer(buffer, "MQTT");

        // protocol level (version 3.1.1)
        buffer.emplace_back(0x4);

        uint8_t connectFlags = 0;
        if(!mConfig.getUsername().empty()) {
            connectFlags |= 0b1000'0000;
        }
        if(!mConfig.getPassword().empty()) {
            connectFlags |= 0b0100'0000;
        }
        if(mConfig.getWillMsg()) {
            connectFlags |= 0b0000'0100;
            if(mConfig.getWillMsg()->isRetained()) {
                connectFlags |= 0b0010'0000;
            }
            if(mConfig.getWillMsg()->getQoS() != QoS::QoS0) {
                assert(false);
            }
        }
        if(mConfig.isCleanSession()) {
            connectFlags |= 0b0000'0010;
        }
        buffer.emplace_back(connectFlags);
        appendUint16_tToBuffer(buffer, mConfig.getKeepAliveIntervalSeconds());

        // payload
        appendString2ByteLengthToBuffer(buffer, mConfig.getClientID());
        if(mConfig.getWillMsg()) {
            appendString2ByteLengthToBuffer(buffer, mConfig.getWillMsg()->getTopic());
            appendString2ByteLengthToBuffer(buffer, mConfig.getWillMsg()->getPayload());
        }
        if(!mConfig.getUsername().empty()) {
            appendString2ByteLengthToBuffer(buffer, mConfig.getUsername());
        }
        if(!mConfig.getPassword().empty()) {
            appendString2ByteLengthToBuffer(buffer, mConfig.getPassword());
        }
        insertMessageLengthToHeader(buffer);
        // send CONNECT
        enqueueSendFront(OnDisconnect::DELETE_ME, std::move(buffer), [this]() -> CallbackStatus {
            // receive CONNACK
            enqueueRecvPacketFront(OnDisconnect::DELETE_ME, [this](MessageType type, const std::vector<uint8_t>& bytes) -> CallbackStatus {
                // handle CONNACK
                if(type != MessageType::CONNACK) {
                    protocolViolation();
                    return CallbackStatus::PROTOCOL_VIOLATION;
                }
                if(bytes.size() != 3) {
                    protocolViolation();
                    return CallbackStatus::PROTOCOL_VIOLATION;
                }
                // idk if we need this, but the flag is inside that bit...
                bool sessionPresent = bytes.at(1) & 1;
                (void)sessionPresent;
                uint8_t errorCode = bytes.at(2);
                if(errorCode != 0) {
                    switch(errorCode) {
                    case 1:
                        throw std::runtime_error{ "Server doesn't support MQTT 3.1.1" };
                    case 2:
                        throw std::runtime_error{ "Client identifier rejected" };
                    case 3:
                        throw std::runtime_error{ "MQTT service unavailable" };
                    case 4:
                        throw std::runtime_error{ "Username or password malformed" };
                    case 5:
                        throw std::runtime_error{ "Client not authorized" };
                    default:
                        protocolViolation();
                        return CallbackStatus::PROTOCOL_VIOLATION;
                    }
                }
                // std::cout << "MQTT connection successfully established!" << std::endl;

                mTries = 0;
                if(mConfig.getOnConnectCallback())
                    mConfig.getOnConnectCallback()();
                return CallbackStatus::OK;
            });
            return CallbackStatus::OK;
        });
        return CallbackStatus::OK;
    }));

    // prepend the connection check task for the tcp connection
    mTaskQueue.emplace_front(std::make_pair(OnDisconnect::DELETE_ME, [this]() -> CallbackStatus {
        auto status = mClient->getConnectState();
        if(status == TCPConnectState::IN_PROGRESS) {
            return CallbackStatus::REPEAT_ME;
        } else if(status == TCPConnectState::CANT_CONNECT) {
            throw std::runtime_error("Can't connect to Server");
        }
        return CallbackStatus::OK;
    }));

    // prepend the tcp connection task
    mTaskQueue.emplace_front(std::make_pair(OnDisconnect::DELETE_ME, [this]() -> CallbackStatus {
        auto now = std::chrono::steady_clock::now();
        if(mConfig.getDelayBetweenRetries() > (now - mLastReconnect)) {
            return CallbackStatus::REPEAT_ME;
        }
        mLastReconnect = std::chrono::steady_clock::now();
        mTries++;
        mClient.emplace(mConfig.getHostname(), mConfig.getPort());
        return CallbackStatus::OK;
    }));
}

void MQTTClient::disconnect() {
    std::lock_guard<std::recursive_mutex> guard{ mMutex };
    if(mDisconnected)
        throw std::runtime_error{ "Already disconnected!" };
    mTaskQueue.emplace_back(std::make_pair(OnDisconnect::KEEP_ME, [this]() -> CallbackStatus {
        std::vector<uint8_t> buffer(2);
        buffer.at(0) = static_cast<uint8_t>(MessageType::DISCONNECT) << 4;
        buffer.at(1) = 0;
        enqueueSendFront(OnDisconnect::KEEP_ME, std::move(buffer), [this]() -> CallbackStatus {
            mDisconnected = true;
            mClient.reset();
            return CallbackStatus::CLEAR_TASK_QUEUE;
        });
        return CallbackStatus::OK;
    }));
}

}
