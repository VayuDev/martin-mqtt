//
// Created by jankl on 29.04.2021.
//

#pragma once

#ifdef __unix__

#include "AbstractTcpClient.hpp"

#include <arpa/inet.h>
#include <exception>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace martin {

class TcpUnix : public AbstractTcpClient {

private:
    int32_t mSocket;

public:
    TcpUnix(const std::string& hostname, uint16_t port);

    ~TcpUnix();

    TcpUnix(const TcpUnix& client) = delete;

    TcpUnix& operator=(const TcpUnix& client) = delete;

    TCPConnectState getConnectState() override;

    ssize_t send(const void* buffer, ssize_t len) override;

    ssize_t recv(void* output, ssize_t len) override;

    bool isDataAvailable() override;

    TcpBlockUntilDataAvailableReturnReason blockUntilDataAvailable(std::optional<std::chrono::milliseconds> timeout) override;
};

}

#endif