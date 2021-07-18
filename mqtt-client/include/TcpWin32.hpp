//
// Created by jankl on 29.04.2021.
//

#pragma once

#ifdef WIN32

#include "WindowsInclude.hpp"
#include "AbstractTcpClient.hpp"
#include <string>

namespace martin {

class TcpWin32 : public AbstractTcpClient {

private:
    SOCKET mSocket;

public:
    TcpWin32(const std::string& hostname, uint16_t port);

    ~TcpWin32();

    TcpWin32(const TcpWin32& client) = delete;

    TcpWin32& operator=(const TcpWin32& client) = delete;

    TCPConnectState getConnectState() override;

    ssize_t send(const void* buffer, ssize_t len) override;

    ssize_t recv(void* output, ssize_t len) override;

    bool isDataAvailable() override;
};

}

#endif