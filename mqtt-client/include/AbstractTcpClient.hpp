#pragma once

#include <string>
#include "Enums.hpp"
#include "WindowsInclude.hpp"
#include <ctime>
#include <thread>
#include <optional>

namespace martin {

#if defined(_MSC_VER)

#include <BaseTsd.h>

typedef SSIZE_T ssize_t;
#endif

class AbstractTcpClient {
protected:
    time_t mLastPacketSendTime = 0;

public:
    AbstractTcpClient() = default;
    virtual ~AbstractTcpClient() = default;
    // As all implementations manage a native socket handle internally, it doesn't make sense to
    // copy this class
    AbstractTcpClient(const AbstractTcpClient&) = delete;
    AbstractTcpClient& operator=(const AbstractTcpClient&) = delete;
    AbstractTcpClient(AbstractTcpClient&&) = delete;
    AbstractTcpClient& operator=(AbstractTcpClient&&) = delete;

    virtual ssize_t send(const void* buffer, ssize_t len) = 0;

    virtual ssize_t recv(void* output, ssize_t len) = 0;

    virtual TCPConnectState getConnectState() = 0;

    virtual bool isDataAvailable() = 0;
    virtual void blockUntilDataAvailable(std::optional<std::chrono::milliseconds> timeout) {
        auto start = std::chrono::steady_clock::now();
        while(getConnectState() == TCPConnectState::CONNECTED && !isDataAvailable()
              && (!timeout || (std::chrono::steady_clock::now() - start) < *timeout)) {
            std::this_thread::yield();
        }
    }

    [[nodiscard]] time_t getSecondsSinceLastSend() const {
        return time(nullptr) - mLastPacketSendTime;
    }
};

}