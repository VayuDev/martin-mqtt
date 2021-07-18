
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#pragma comment(lib, "Ws2_32.lib")

#include "AbstractTcpClient.hpp"
#include <cassert>
#include <exception>
#include <stdexcept>
#include <string>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctime>

#ifndef ssize_t
#define ssize_t SSIZE_T
#endif

#endif