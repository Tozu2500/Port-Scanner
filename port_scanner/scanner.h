#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>

struct ScanResult {
    int port;
    bool open;
    std::string service;
    std::string banner;
    double responseMs;
};

struct ScanConfig {
    std::string host;
    int startPort = 1;
    int endPort = 1024;
    int threads = 100;
    int timeoutMs = 1500;
    bool grabBanners = true;
    int bannerTimeoutMs = 2000;
};

using ProgressCallback = std::function<void(int scanned, int total, const ScanResult&)>;

std::vector<ScanResult> runScan(
    const ScanConfig& cfg,
    std::atomic<bool>& cancelFlag,
    ProgressCallback onResult
);