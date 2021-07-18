#pragma once

#include "TcpUnix.hpp"
#include "TcpWin32.hpp"

namespace martin {

#ifdef __unix__
using TcpClient = TcpUnix;
#elif WIN32
using TcpClient = TcpWin32;
#else
#error "Your OS is not supported."
#endif

}