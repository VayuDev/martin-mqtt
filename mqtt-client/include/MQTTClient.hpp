#pragma once

#include "TcpClient.hpp"

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

#include "AbstractTcpClient.hpp"
#include "Enums.hpp"
#include "Forward.hpp"
#include "MQTTConfigBuilder.hpp"
#include "MQTTMessage.hpp"
#include "AbstractMQTTMessageHandler.hpp"
#include <list>
#include <variant>
#include <memory>

namespace martin {

using MQTTPublishCallback = std::variant<std::function<void(const MQTTMessage&)>, std::shared_ptr<AbstractMQTTMessageHandler>>;

class MQTTClient {
private:
    std::optional<TcpClient> mClient;
    const MQTTConfig mConfig;
    std::list<std::pair<OnDisconnect, std::function<CallbackStatus()>>> mTaskQueue;
    std::vector<std::pair<std::string, MQTTPublishCallback>> mCallbacks;
    std::recursive_mutex mMutex;
    // This is set while receiving a packet to prevent enqueueing an empty receive-packet once there is data
    // on the server (which is otherwise useful for receiving publish).
    bool mCurrentlyReceivingPacket = false;
    // Once disconnect, all external function calls throw exceptions
    bool mDisconnected = false;
    // Counts how many times we tried to reconnect.
    int mTries = 0;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> mLastReconnect;

    void connectMqtt();
    /**
     * This function simply disconnects from the remote server by resetting mClient.
     * It is often called by tasks and by loop, this makes debugging easier as you can simply place
     * a breakpoint in this function and see where the problem is. This wouldn't be possible if this function
     * was only called by loop.
     */
    void protocolViolation();
    void enqueueSendFront(OnDisconnect shouldKeep, std::vector<uint8_t> buffer, std::function<CallbackStatus()> onDone);
    /**
     * This function is called when we receive a packet (expecting one or not) and got a PUBLISH packet.
     * It calls all the registered handlers.
     */
    void handlePublishReceived(MessageType messageType, const std::vector<uint8_t>& buffer);

    // the buffer passed to onDone contains all received bytes except for the var-length encoded
    // packet size
    void enqueueRecvPacketFront(OnDisconnect shouldKeep, std::function<CallbackStatus(MessageType, const std::vector<uint8_t>&)> onDone);

    uint16_t mPacketIdentifierCounter = 1;

    void enqueueSendPingReq();

public:
    /**
     * Constructs a MQTTClient with the specified configuration.
     * The configuration is stored in the object and reused upon reconnecting.
     *
     * To send messages, you can use MQTTClient::publish or operator<<.
     * To subscribe to a topic, use the MQTTClient::subscribe function.
     *
     * This class is fully asynchronous. This means that you can always subscribe to topics, even if
     * there is no connection at the current point in time; the client will automatically subscribe
     * to all previously specified topics upon (re-)connecting successfully.
     *
     * <br>
     * <br>
     *
     * All member functions are non-blocking, except for MQTTClient::loop.
     *
     * All methods are fully thread-safe and can be called from any thread.
     *
     * @param config The configuration to use.
     */
    explicit MQTTClient(MQTTConfig config);

    virtual ~MQTTClient() = default;

    /**
     * Enqueues a message to published to the broker.
     * <br><br>
     * The message will only be sent once MQTTClient::loop is called.
     * This function is non-blocking and thread-safe.
     *
     * @param msg The message to be published.
     */
    MQTTClient& operator<<(MQTTMessage msg);

    /**
     * Enqueues a message to published to the broker.
     * <br><br>
     * The message will only be sent once MQTTClient::loop is called.
     * This function is non-blocking and thread-safe.
     *
     * @param msg The message to be published.
     */
    void publish(MQTTMessage msg);

    /**
     * Subscribes to a topic.
     * <br><br>
     * The packet to the broker will only be sent once MQTTClient::loop is called.
     * This function is non-blocking and thread-safe.
     *
     * @param topic The topic to subscribe to.
     * @param qos The QoS to use (only QoS 0 is supported).
     * @param callback The function to call upon receiving a message.
     */
    void subscribe(std::string topic, QoS qos, MQTTPublishCallback callback);

    /**
     * Unsubscribes from a topic. This will also delete all registered handlers for that topic
     * immediately. It's possible that we receive messages for that topic before the server receives
     * the unsubscribe packet, in which case those messages will be dropped. <br><br> The packet to
     * the broker will only be sent once MQTTClient::loop is called. This function is non-blocking
     * and thread-safe.
     *
     * @param topic The topic to unsubscribe from.
     */
    void unsubscribe(std::string topic);

    /**
     * Disconnects cleanly from the server.
     * If you don't call this method before destructing this object, the TCP connection will just be
     * dropped and the broker will send the will-message specified in MQTTConfig.
     *
     * <br><br>
     * The packet to the broker will only be sent once MQTTClient::loop is called.
     * This function is non-blocking and thread-safe.
     *
     * <br><br>
     * This means means you have to wait until MQTTClient::loop returns
     * MQTTClientLoopStatus::EVERYTHING_DONE or until MQTTClient::isDisconnected returns true until
     * you can safely destruct this object; otherwise the DISCONNECT packet may not be received by
     * the broker.
     *
     * @param topic The topic to unsubscribe from.
     */
    void disconnect();

    /**
     * Blocks the current thread for no longer than the duration specified by timeout.
     *
     *
     *
     * <br><br>
     * This function is thread-safe.
     *
     * @param timeout The maximum blocking duration of this function. If the optional has no value,
     * the function will block until all tasks have been handled. If the optional has the value 0ms,
     * exactly one task will be handled.
     * @param useSelect If useSelect is set to BlockForRecv::Yes, then this function will use select/poll. This means
     * that it will block about timeout milliseconds and indefinetely if timeout is empty.
     * @return The reason why this function returned.
     */
    MQTTClientLoopStatus loop(std::optional<std::chrono::milliseconds> timeout, BlockForRecv useSelect = BlockForRecv::No);

    /**
     * Get the number of tasks that are currently in the task queue. This function is mainly used
     * for debugging purposes and can be used to see if the MQTTClient is overloaded.
     *
     * @note The number of tasks enqueued per publish/subscribe is an implementation detail. Some
     * tasks like PING are enqueued automatically.
     *
     * @return The number of tasks in the task queue.
     */
    size_t getQueuedTaskCount();

    /**
     * Checks wether or not the client is disconnected from the broker. Calling any function other
     * than loop while dis- connected is an error. This function will return true after
     * MQTTClient::disconnect has sent it's packet to the server.
     *
     * @return True if disconnected from the broker, false otherwise.
     */
    bool isDisconnected() {
        std::lock_guard<std::recursive_mutex> lock{ mMutex };
        return mDisconnected;
    }
};

}
