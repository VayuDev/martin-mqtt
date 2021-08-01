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
#include <poll.h>
#include <iostream>

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

    mSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if(mSocket <= 0) {
        throw std::runtime_error("Could not create Socket");
    }
    DestructWrapper destructor([this] {
        if(close(mSocket) < 0) {
            perror("close()");
        }
    });

    // Set some small timeouts to increase throughtput. Settings theses values to 0 increases CPU usage under slow networks to 100%,
    // though it also ensures that timeouts are kept more strictly.
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;

    if(setsockopt(mSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
        throwError("setsockopt()");
    }
    if(setsockopt(mSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
        throwError("setsockopt()");
    }

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

    // Try to increase buffer size, increases performance; we don't really care if it doesn't work.
#ifdef F_SETPIPE_SZ
    fcntl(mSocket, F_SETPIPE_SZ, 1024 * 64 * 4);
#endif

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

TcpBlockUntilDataAvailableReturnReason TcpUnix::blockUntilDataAvailable(std::optional<std::chrono::milliseconds> timeout) {
    int pollTimeout = timeout.has_value() ? (int)timeout.value().count() : -1;
    pollfd fd;
    fd.fd = mSocket;
    fd.events = POLLIN;
    fd.revents = 0;
    auto ret = poll(&fd, 1, pollTimeout);
    if(ret == 0) {
        return TcpBlockUntilDataAvailableReturnReason::TIMEOUT;
    }
    if(ret < 0) {
        throwError("poll()");
    }
    if(fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        throw std::runtime_error{ "poll() detected socket error" };
    }
    return TcpBlockUntilDataAvailableReturnReason::DATA_AVAILABLE;
}

}

#endif
