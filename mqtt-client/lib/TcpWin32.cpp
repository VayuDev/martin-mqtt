
#ifdef WIN32

#include "TcpWin32.hpp"

#include <iostream>
#include "Helper.hpp"

namespace martin {

static void throwWSAError(const std::string& function) {
    wchar_t errorMsg[512];
    errorMsg[0] = '\0';
    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&errorMsg, 512, nullptr);
    std::wstring errorString = errorMsg;
    throw std::runtime_error(function + ": " + std::string(errorString.begin(), errorString.end()));
}

TcpWin32::TcpWin32(const std::string& hostname, uint16_t port) {
    // View https://docs.microsoft.com/de-de/windows/win32/winsock/complete-client-code for
    // reference

    mSocket = INVALID_SOCKET;
    struct addrinfo *result = nullptr, hints{};

    int iResult;
    WSADATA wsa;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsa);
    if(iResult != 0) {
        throwWSAError("WSAStartup()");
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    auto portStr = std::to_string(port);
    iResult = getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &result);
    if(iResult != 0) {
        throwWSAError("getaddrinfo()");
    }

    DestructWrapper wrapper([&] { freeaddrinfo(result); });

    if(result != nullptr) {
        mSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if(mSocket == INVALID_SOCKET) {
            throwWSAError("socket()");
        }
        u_long mode = 1;
        if(ioctlsocket(mSocket, FIONBIO, &mode) != NO_ERROR) {
            throwWSAError("ioctlsocket()");
        }
        iResult = connect(mSocket, result->ai_addr, (int)result->ai_addrlen);
        if(iResult == SOCKET_ERROR) {
            if(WSAGetLastError() != WSAEWOULDBLOCK) {
                closesocket(mSocket);
                mSocket = INVALID_SOCKET;
            }
        }
    }

    if(mSocket == INVALID_SOCKET) {
        WSACleanup();
        throw std::runtime_error("WSACleanup(): No open connection found for configuration");
    }
}

TcpWin32::~TcpWin32() {
    auto result = closesocket(mSocket);
    if(WSACleanup() != 0) {
        throwWSAError("WSACleanup()");
    }
    if(result != 0) {
        std::cerr << "Cant close socket: " << result << std::endl;
    }
}

TCPConnectState TcpWin32::getConnectState() {
    fd_set write_set;
    struct timeval timeout = { 0, 1 };

    FD_ZERO(&write_set);
    FD_SET(mSocket, &write_set);
    auto result = select(mSocket, 0, &write_set, 0, &timeout);
    if(result == SOCKET_ERROR) {
        if(WSAGetLastError() == WSAEINPROGRESS) {
            return TCPConnectState::IN_PROGRESS;
        } else {
            return TCPConnectState::CANT_CONNECT;
        }
    } else if(result == 0) {
        return TCPConnectState::IN_PROGRESS;
    }
    return TCPConnectState::CONNECTED;
}

ssize_t TcpWin32::send(const void* buffer, ssize_t len) {
    auto result = ::send(mSocket, static_cast<const char*>(buffer), len, 0);
    if(result == SOCKET_ERROR) {
        if(WSAGetLastError() != WSAEWOULDBLOCK) {
            throwWSAError("send()");
        } else {
            result = 0;
        }
    }
    mLastPacketSendTime = time(nullptr);
    return result;
}

ssize_t TcpWin32::recv(void* output, ssize_t len) {
    auto result = ::recv(mSocket, static_cast<char*>(output), len, 0);
    if(result == SOCKET_ERROR) {
        if(WSAGetLastError() != WSAEWOULDBLOCK) {
            throwWSAError("recv()");
        } else {
            result = 0;
        }
    } else if(result == 0 && WSAGetLastError() != 0) {
        throwWSAError("recv()");
    }
    return result;
}

// TODO implement this method :)
bool TcpWin32::isDataAvailable() {
    u_long bytesToRead = 0;
    if(ioctlsocket(mSocket, FIONBIO, &bytesToRead) != NO_ERROR) {
        throwWSAError("ioctlsocket()");
    }
    return bytesToRead > 0;
}
}

#endif