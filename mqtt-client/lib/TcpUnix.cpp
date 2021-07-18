//
// Created by jankl on 29.04.2021.
//

#ifdef __unix__

#include "TcpUnix.hpp"
#include "Helper.hpp"
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <cerrno>
#include <cassert>

namespace martin {
static void throwError(const std::string& function) {
    char errorBuffer[512] = { 0 };
    strerror_r(static_cast<int>(errno), errorBuffer, 512);
    throw std::runtime_error(function + ": " + errorBuffer);
}

static void throwHError(const std::string& function) {
    throw std::runtime_error(function + ": " + hstrerror(h_errno));
}

TcpUnix::TcpUnix(const std::string& hostname, uint16_t port) {

    mSocket = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(mSocket <= 0) {
        throw std::runtime_error("Could not create Socket");
    }
    DestructWrapper destructor([this] {
        if(close(mSocket) < 0) {
            perror("close()");
        }
    });

    struct in_addr** addr_list;
    struct hostent* hostent;
    struct sockaddr_in connInfo { };

    hostent = gethostbyname(hostname.c_str());
    if(hostent == nullptr) {
        throwHError("gethostbyname()");
    }

    addr_list = (struct in_addr**)hostent->h_addr_list;
    if(hostent->h_length > 0) {
        connInfo.sin_addr = *addr_list[0];
    } else {
        throw std::runtime_error("Host address could not be resolved");
    }

    connInfo.sin_port = htons(port);
    connInfo.sin_family = AF_INET;

    auto result = ::connect(mSocket, (struct sockaddr*)&connInfo, sizeof(connInfo));
    if(result < 0 && errno != EINPROGRESS) {
        throwError("connect()");
    }

    // Set non-blocking
    int blockState = 0;
    if((blockState = fcntl(mSocket, F_GETFL, NULL)) < 0) {
        throwHError("fcntl()");
    }
    // blockState |= O_NONBLOCK;
    // if(fcntl(mSocket, F_SETFL, blockState) < 0) {
    //    throwHError("fcntl()");
    //}

    destructor.disarm();
}

TcpUnix::~TcpUnix() {
    if(close(mSocket) < 0) {
        perror("close()");
    }
}

TCPConnectState TcpUnix::getConnectState() {
    int error = 0;
    socklen_t len = sizeof(int);
    getsockopt(mSocket, SOL_SOCKET, SO_ERROR, &error, &len);
    if(error == EINPROGRESS) {
        return TCPConnectState::IN_PROGRESS;
    } else if(error != 0) {
        return TCPConnectState::CANT_CONNECT;
    }
    return TCPConnectState::CONNECTED;
}

ssize_t TcpUnix::send(const void* buffer, ssize_t len) {
    auto result = ::send(mSocket, buffer, len, MSG_NOSIGNAL);
    if(result < 0) {
        if(errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;
        }
        throwError("send()");
    }
    mLastPacketSendTime = time(nullptr);
    return result;
}

ssize_t TcpUnix::recv(void* output, ssize_t len) {
    auto result = ::recv(mSocket, output, len, MSG_NOSIGNAL);
    if(result <= 0) {
        if(errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;
        }
        throwError("recv()");
    }
    return result;
}
bool TcpUnix::isDataAvailable() {
    int numBytes = -1;
    if(ioctl(mSocket, FIONREAD, &numBytes) < 0) {
        throwError("ioctl()");
    }
    return numBytes > 0;
}

void TcpUnix::blockUntilDataAvailable(std::optional<std::chrono::milliseconds> timeout) {
    fd_set fdSet;
    FD_ZERO(&fdSet);
    FD_SET(mSocket, &fdSet);
    if(timeout) {
        timeval selectTimeout{};
        selectTimeout.tv_sec = timeout->count() / 1000;
        selectTimeout.tv_usec = (timeout->count() % 1000) * 1000;
        select(mSocket + 1, &fdSet, nullptr, nullptr, &selectTimeout);
    } else {
        select(mSocket + 1, &fdSet, nullptr, nullptr, nullptr);
    }
}

}

#endif
