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