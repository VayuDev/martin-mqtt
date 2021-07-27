#pragma once

#include <cstdint>

namespace martin {

enum class Retain
{
    Yes,
    No
};

enum class QoS : uint8_t
{
    QoS0 = 0,
    QoS1 = 1,
    QoS2 = 2
};

enum class CleanSession
{
    No,
    Yes
};

enum class BlockForRecv
{
    Yes,
    No
};

enum class CallbackStatus
{
    OK,
    PROTOCOL_VIOLATION,
    REPEAT_ME,
    CLEAR_TASK_QUEUE
};

enum class MessageType : uint8_t
{
    INVALID = 0,
    CONNECT = 1,
    CONNACK = 2,
    PUBLISH = 3,
    PUBREL = 6,
    SUBSCRIBE = 8,
    SUBACK = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK = 11,
    PINGREQ = 12,
    PINGRESP = 13,
    DISCONNECT = 14
};

enum class MQTTClientLoopStatus
{
    TIMEOUT,
    NOTHING_TO_DO,
    EVERYTHING_DONE,
    CANT_CONNECT,
    EXCEPTION_AFTER_DISCONNECT
};

enum class OnDisconnect
{
    KEEP_ME,
    DELETE_ME,
};

enum class TCPConnectState
{
    CANT_CONNECT,
    IN_PROGRESS,
    CONNECTED
};

enum class TcpBlockUntilDataAvailableReturnReason
{
    TIMEOUT,
    DATA_AVAILABLE
};

}
