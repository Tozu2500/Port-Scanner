#include "scanner.h"
#include "services.h"

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <string>
#include <atomic>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

// Helper functions
static bool resolveHost(const std::string& host, sockaddr_in& addr) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;

    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res)
        return false;

    addr = *reinterpret_cast<sockaddr_in*>(res->ai_addr);
    freeaddrinfo(res);
    return true;
}

// Connect with milliseconds timeout using nonblocking socket + select
// This returns TRUE if the port is open; fills responseMs
static bool tcpConnect(const sockaddr_in& baseAddr, int port, int timeoutMs, double& responseMs) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (s == INVALID_SOCKET) {
        return false;
    }

    // Non Blocking mode
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    sockaddr_in addr = baseAddr;
    addr.sin_port = htons(static_cast<u_short>(port));

    auto t0 = std::chrono::steady_clock::now();
    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    fd_set wset, eset;
    FD_ZERO(&wset);
    FD_SET(s, &wset);

    FD_ZERO(&eset);
    FD_SET(s, &eset);

    TIMEVAL tv;

    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int r = select(0, nullptr, &wset, &eset, &tv);

    bool open = false;

    if (r > 0 && FD_ISSET(s, &wset)) {
        // Here verify the connection truly completed, no pending errors.
        int err = 0;
        int len = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
        open = (err == 0);
    }

    auto t1 = std::chrono::steady_clock::now();
    responseMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    closesocket(s);
    return open;
}

