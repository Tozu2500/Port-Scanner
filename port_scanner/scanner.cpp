#include "scanner.h"
#include "services.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")


//  DNS resolution

// Turns a hostname or IP string into a sockaddr_in we can connect to.
// Returns false if the host cannot be resolved.
static bool resolveHost(const std::string& host, sockaddr_in& outAddr)
{
    addrinfo hints{};
    hints.ai_family   = AF_INET;       // IPv4 only
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* results = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &results) != 0 || !results)
        return false;

    outAddr = *reinterpret_cast<sockaddr_in*>(results->ai_addr);
    freeaddrinfo(results);
    return true;
}

//  TCP connect probe

// Attempts a TCP connection to the given port within timeoutMs milliseconds.
// Uses a non-blocking socket + select() so we never block a thread for longer
// than the timeout, even if the remote host simply drops the packet.
// Returns true if the port is open; always fills outResponseMs.
static bool tcpConnect(const sockaddr_in& baseAddr,
                       int                port,
                       int                timeoutMs,
                       double&            outResponseMs)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return false;

    // Switch to non-blocking so connect() returns immediately
    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);

    // Fill in the target port
    sockaddr_in target = baseAddr;
    target.sin_port    = htons(static_cast<u_short>(port));

    auto startTime = std::chrono::steady_clock::now();

    // connect() will return WSAEWOULDBLOCK — that is expected
    connect(sock, reinterpret_cast<sockaddr*>(&target), sizeof(target));

    // Wait for the socket to become writable, which means the connection
    // either succeeded or was refused
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(sock, &writeSet);

    TIMEVAL timeout;
    timeout.tv_sec  = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    bool portIsOpen = false;

    if (select(0, nullptr, &writeSet, nullptr, &timeout) > 0
        && FD_ISSET(sock, &writeSet))
    {
        // The socket became writable, but we must check SO_ERROR to distinguish
        // a successful connection from a refused one
        int  socketError = 0;
        int  optLen      = sizeof(socketError);
        getsockopt(sock, SOL_SOCKET, SO_ERROR,
                   reinterpret_cast<char*>(&socketError), &optLen);

        portIsOpen = (socketError == 0);
    }

    auto endTime   = std::chrono::steady_clock::now();
    outResponseMs  = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    closesocket(sock);
    return portIsOpen;
}

//  Banner grabbing

// Returns true if this port number is an HTTP variant that needs a request
// before it will send anything back
static bool isHttpPort(int port)
{
    return port == 80 || port == 443 || port == 8080 || port == 8443;
}

// Cleans up raw socket bytes into a printable single-line string.
// Replaces control characters with spaces, collapses runs of whitespace,
// and trims both ends.
static std::string cleanBannerText(const char* buf, int length)
{
    // Replace non-printable bytes with spaces
    std::string text;
    text.reserve(static_cast<size_t>(length));

    for (int i = 0; i < length; ++i)
    {
        unsigned char byte = static_cast<unsigned char>(buf[i]);
        bool isPrintable   = (byte >= 0x20 && byte < 0x7F);
        bool isNewline     = (byte == '\n' || byte == '\r');

        if      (isPrintable) text += static_cast<char>(byte);
        else if (isNewline)   text += ' ';
    }

    // Collapse multiple spaces into one
    std::string collapsed;
    bool inSpace = false;

    for (char ch : text)
    {
        if (ch == ' ')
        {
            if (!inSpace) { collapsed += ' '; inSpace = true; }
        }
        else
        {
            collapsed += ch;
            inSpace = false;
        }
    }

    // Trim trailing space
    while (!collapsed.empty() && collapsed.back() == ' ')
        collapsed.pop_back();

    // Truncate to a display-friendly length
    const size_t maxLength = 80;
    if (collapsed.size() > maxLength)
        collapsed = collapsed.substr(0, maxLength - 3) + "...";

    return collapsed;
}

// Opens a fresh blocking connection to the port and reads the server's
// opening message (e.g. "SSH-2.0-OpenSSH_8.9", "220 mail.example.com").
// HTTP ports get a HEAD request first because they don't speak until asked.
// Returns an empty string if nothing is received within timeoutMs.
static std::string grabBanner(const sockaddr_in& baseAddr,
                               int                port,
                               int                timeoutMs)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return "";

    // Set a hard deadline on both send and receive
    DWORD timeoutDword = static_cast<DWORD>(timeoutMs);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<char*>(&timeoutDword), sizeof(timeoutDword));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<char*>(&timeoutDword), sizeof(timeoutDword));

    sockaddr_in target = baseAddr;
    target.sin_port    = htons(static_cast<u_short>(port));

    if (connect(sock, reinterpret_cast<sockaddr*>(&target), sizeof(target)) != 0)
    {
        closesocket(sock);
        return "";
    }

    // HTTP servers wait for a request; everything else (SSH, FTP, SMTP ...)
    // sends a greeting as soon as the connection opens
    if (isHttpPort(port))
    {
        const char* httpProbe = "HEAD / HTTP/1.0\r\n\r\n";
        send(sock, httpProbe, static_cast<int>(strlen(httpProbe)), 0);
    }

    char rawBuffer[257]{};
    int  bytesReceived = recv(sock, rawBuffer, 256, 0);
    closesocket(sock);

    if (bytesReceived <= 0)
        return "";

    return cleanBannerText(rawBuffer, bytesReceived);
}


//  Thread pool

// A simple fixed-size thread pool.  Workers pull tasks from a shared queue
// and run them.  The pool is destroyed (and all threads joined) when it goes
// out of scope.
class ThreadPool
{
public:
    explicit ThreadPool(int threadCount)
    {
        for (int i = 0; i < threadCount; ++i)
            workers_.emplace_back([this] { workerLoop(); });
    }

    ~ThreadPool()
    {
        // Signal all workers to finish and wait for them
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            shuttingDown_ = true;
        }
        workReady_.notify_all();

        for (std::thread& worker : workers_)
            worker.join();
    }

    void enqueue(std::function<void()> task)
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            taskQueue_.push(std::move(task));
        }
        workReady_.notify_one();
    }

private:
    void workerLoop()
    {
        while (true)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                workReady_.wait(lock, [this]
                {
                    return shuttingDown_ || !taskQueue_.empty();
                });

                if (shuttingDown_ && taskQueue_.empty())
                    return;

                task = std::move(taskQueue_.front());
                taskQueue_.pop();
            }

            task();
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> taskQueue_;
    std::mutex                        queueMutex_;
    std::condition_variable           workReady_;
    bool                              shuttingDown_ = false;
};


//  Public entry point

std::vector<ScanResult> runScan(const ScanConfig&  cfg,
                                std::atomic<bool>& cancelFlag,
                                ProgressCallback   onResult)
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Resolve the hostname once, then reuse the address for every port
    sockaddr_in baseAddr{};
    if (!resolveHost(cfg.host, baseAddr))
    {
        WSACleanup();
        return {};
    }

    const int        totalPorts   = cfg.endPort - cfg.startPort + 1;
    std::atomic<int> portsScanned { 0 };

    std::vector<ScanResult> openResults;
    std::mutex              openResultsMutex;

    // Scope the pool so its destructor blocks until all tasks finish
    {
        int poolSize = std::min(cfg.threads, totalPorts);
        ThreadPool pool(poolSize);

        for (int port = cfg.startPort; port <= cfg.endPort; ++port)
        {
            if (cancelFlag) break;

            pool.enqueue([&, port]
            {
                if (cancelFlag) return;

                // 1. Test whether the port is open
                double responseMs = 0.0;
                bool   isOpen     = tcpConnect(baseAddr, port, cfg.timeoutMs, responseMs);

                // 2. Build the result record
                ScanResult result;
                result.port       = port;
                result.open       = isOpen;
                result.service    = lookupService(port);
                result.responseMs = responseMs;

                // 3. Optionally grab the service banner
                if (isOpen && cfg.grabBanners)
                    result.banner = grabBanner(baseAddr, port, cfg.bannerTimeoutMs);

                // 4. Report progress to the caller
                int scannedSoFar = ++portsScanned;
                onResult(scannedSoFar, totalPorts, result);

                // 5. Store open ports for the final report
                if (isOpen)
                {
                    std::lock_guard<std::mutex> lock(openResultsMutex);
                    openResults.push_back(result);
                }
            });
        }
    } // ThreadPool destructor joins all workers here

    WSACleanup();

    // Sort by port number so the results table is in a logical order
    std::sort(openResults.begin(), openResults.end(),
              [](const ScanResult& a, const ScanResult& b)
              {
                  return a.port < b.port;
              });

    return openResults;
}