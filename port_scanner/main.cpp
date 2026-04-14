#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "scanner.h"
#include "ui.h"


//  String helpers

// Removes leading and trailing whitespace from a string
static std::string trim(std::string s)
{
    size_t first = s.find_first_not_of(" \t\r\n");
    size_t last  = s.find_last_not_of(" \t\r\n");
    return (first == std::string::npos) ? "" : s.substr(first, last - first + 1);
}

// Replaces characters that are not allowed in Windows filenames
static std::string makeSafeFilename(std::string name)
{
    for (char& c : name)
        if (c == ':' || c == '/' || c == '\\' || c == '*' || c == '?' || c == '"')
            c = '_';
    return name;
}


//  Console prompts

// Prints a labelled prompt and reads one line from the user.
// If the user presses Enter without typing, the default value is returned.
static std::string prompt(const char* label, const std::string& defaultValue = "")
{
    if (defaultValue.empty())
        printf("  %s%s%s : ", ansi::BYELLOW, label, ansi::RESET);
    else
        printf("  %s%s%s [%s%s%s] : ",
               ansi::BYELLOW, label,         ansi::RESET,
               ansi::BBLACK,  defaultValue.c_str(), ansi::RESET);

    char buf[512]{};
    if (!fgets(buf, sizeof(buf), stdin))
        return defaultValue;

    std::string input = trim(buf);
    return input.empty() ? defaultValue : input;
}

// Prompts for an integer and keeps asking until the user enters a valid one
// within [lo, hi]
static int promptInt(const char* label, int defaultValue, int lo, int hi)
{
    std::string defaultHint = std::to_string(defaultValue)
                            + ", range " + std::to_string(lo)
                            + "-"        + std::to_string(hi);
    while (true)
    {
        std::string input = prompt(label, defaultHint);

        // Strip the hint text if the user just pressed Enter
        size_t commaPos = input.find(',');
        if (commaPos != std::string::npos)
            input = input.substr(0, commaPos);

        try
        {
            int value = std::stoi(input);
            if (value >= lo && value <= hi)
                return value;
        }
        catch (...) {}

        printf("  %sPlease enter a number between %d and %d.%s\n",
               ansi::BRED, lo, hi, ansi::RESET);
    }
}

// Prompts for a yes/no answer; accepts y/n/yes/no (case-insensitive)
static bool promptBool(const char* label, bool defaultValue)
{
    std::string input = prompt(label, defaultValue ? "yes" : "no");
    char        first = input.empty() ? (defaultValue ? 'y' : 'n')
                                      : static_cast<char>(tolower(input[0]));
    return first == 'y';
}


//  Header and separators

static void printSeparator(char c = '-', int width = 66)
{
    printf("%s", ansi::BBLACK);
    for (int i = 0; i < width; ++i)
        putchar(c);
    printf("%s\n", ansi::RESET);
}

static void printHeader()
{
    printf("\n");
    printf("%s%s", ansi::BCYAN, ansi::BOLD);
    printf("  +-------------------------------------------------+\n");
    printf("  |                                                 |\n");
    printf("  |    %s PORT SCANNER%s%s%s   //  TCP / Banner / MT  %s    |\n",
           ansi::BWHITE, ansi::RESET, ansi::BCYAN, ansi::BOLD, ansi::RESET);
    printf("%s%s", ansi::BCYAN, ansi::BOLD);
    printf("  |                                                 |\n");
    printf("  +-------------------------------------------------+\n");
    printf("%s\n", ansi::RESET);
}


//  Results table

// Picks a colour for the port number based on its range:
//   well-known (0-1023)    = red
//   registered (1024-49151) = yellow
//   dynamic/ephemeral       = green
static const char* portColour(int port)
{
    if (port <= 1023)  return ansi::BRED;
    if (port <= 49151) return ansi::BYELLOW;
    return ansi::BGREEN;
}

static void printResultsTable(const std::vector<ScanResult>& results)
{
    printf("\n");
    printSeparator('=');
    printf("  %s%s  %-6s  %-16s  %-8s  %s%s\n",
           ansi::BOLD, ansi::BWHITE,
           "PORT", "SERVICE", "RESP(ms)", "BANNER",
           ansi::RESET, "");
    printSeparator();

    for (const ScanResult& r : results)
    {
        const char* bannerText = r.banner.empty() ? "(no banner)" : r.banner.c_str();

        printf("  %s%s%-6d%s  %s%-16s%s  %s%7.1f%s   %s%s%s\n",
               ansi::BOLD,   portColour(r.port), r.port,         ansi::RESET,
               ansi::BCYAN,  r.service.c_str(),                  ansi::RESET,
               ansi::BBLACK, r.responseMs,                       ansi::RESET,
               ansi::BWHITE, bannerText,                         ansi::RESET);
    }

    printSeparator();
    printf("  %s%zu open port%s found.%s\n\n",
           ansi::BGREEN,
           results.size(),
           results.size() == 1 ? "" : "s",
           ansi::RESET);
}


//  File export

static void exportResultsToFile(const ScanConfig&              cfg,
                                const std::vector<ScanResult>& results,
                                double                         elapsedSec)
{
    std::string filename = makeSafeFilename("scan_" + cfg.host + ".txt");

    FILE* file = fopen(filename.c_str(), "w");
    if (!file)
    {
        printf("  Could not write to %s\n", filename.c_str());
        return;
    }

    // Header block
    fprintf(file, "Port Scanner Report\n");
    fprintf(file, "===================\n");
    fprintf(file, "Host        : %s\n",  cfg.host.c_str());
    fprintf(file, "Range       : %d - %d\n", cfg.startPort, cfg.endPort);
    fprintf(file, "Threads     : %d\n",  cfg.threads);
    fprintf(file, "Timeout     : %d ms\n", cfg.timeoutMs);
    fprintf(file, "Banners     : %s\n",  cfg.grabBanners ? "yes" : "no");
    fprintf(file, "Elapsed     : %.2f s\n", elapsedSec);
    fprintf(file, "Open ports  : %zu\n\n", results.size());

    // Results table
    fprintf(file, "%-6s  %-18s  %-8s  %s\n", "PORT", "SERVICE", "RESP(ms)", "BANNER");
    fprintf(file, "%s\n", std::string(80, '-').c_str());

    for (const ScanResult& r : results)
    {
        fprintf(file, "%-6d  %-18s  %7.1f   %s\n",
                r.port,
                r.service.c_str(),
                r.responseMs,
                r.banner.c_str());
    }

    fclose(file);
    printf("  %sResults saved to %s%s%s\n\n",
           ansi::BGREEN, ansi::BWHITE, filename.c_str(), ansi::RESET);
}


//  Live progress display

// Holds shared state that the progress callback reads and writes.
// The mutex protects openCount and lastFoundLine from concurrent access.
struct ProgressState
{
    std::mutex  mutex;
    int         openCount    = 0;
    int         totalPorts   = 0;
    std::string lastFoundLine;     // description of the most recently found port
};

// Redraws the 3-line progress block in-place using ANSI cursor movement.
// Must be called with ps.mutex already held.
static void redrawProgress(const ProgressState& ps, int scanned, const std::string& foundLine)
{
    double percent = ps.totalPorts > 0
                   ? (100.0 * scanned / ps.totalPorts)
                   : 0.0;

    // Line 1 — progress bar + percentage
    ansi::clearLine();
    printf("  [%s] %s%.1f%%%s  %s%d%s/%d\n",
           progressBar(scanned, ps.totalPorts, 35).c_str(),
           ansi::BWHITE,  percent,         ansi::RESET,
           ansi::BYELLOW, scanned,         ansi::RESET,
           ps.totalPorts);

    // Line 2 — last discovered port (or a placeholder while nothing found yet)
    ansi::clearLine();
    if (!foundLine.empty())
        printf("  %s>>> %s%s\n", ansi::BGREEN, foundLine.c_str(), ansi::RESET);
    else
        printf("  %s(scanning...)%s\n", ansi::BBLACK, ansi::RESET);

    // Line 3 — running total of open ports
    ansi::clearLine();
    printf("  %sOpen ports so far: %s%d%s\n",
           ansi::BBLACK, ansi::BGREEN, ps.openCount, ansi::RESET);

    fflush(stdout);
}

// Builds the human-readable description shown when a port is found open
static std::string formatFoundLine(const ScanResult& result)
{
    std::ostringstream line;
    line << "Port " << result.port
         << " (" << result.service << ")"
         << "  " << std::fixed << std::setprecision(1) << result.responseMs << " ms";

    if (!result.banner.empty())
        line << "  |  " << result.banner;

    return line.str();
}


int main()
{
    enableAnsi();
    ansi::hideCursor();

    printHeader();

    // 1. Collect scan settings from the user

    printf("  %sScan Configuration%s\n\n", ansi::BOLD, ansi::RESET);

    ScanConfig cfg;
    cfg.host          = prompt("Target host / IP", "scanme.nmap.org");
    cfg.startPort     = promptInt("Start port",       1,    1, 65535);
    cfg.endPort       = promptInt("End port",         1024, cfg.startPort, 65535);
    cfg.threads       = promptInt("Threads",          150,  1, 500);
    cfg.timeoutMs     = promptInt("Timeout (ms)",     1200, 100, 10000);
    cfg.grabBanners   = promptBool("Grab banners?",   true);

    if (cfg.grabBanners)
        cfg.bannerTimeoutMs = promptInt("Banner timeout (ms)", 2000, 200, 10000);

    int totalPorts = cfg.endPort - cfg.startPort + 1;

    printf("\n");
    printSeparator('=');
    printf("  Scanning %s%s%s  ports %s%d%s-%s%d%s  (%d ports, %d threads)\n",
           ansi::BCYAN,   cfg.host.c_str(),  ansi::RESET,
           ansi::BYELLOW, cfg.startPort,     ansi::RESET,
           ansi::BYELLOW, cfg.endPort,       ansi::RESET,
           totalPorts, cfg.threads);
    printSeparator('=');
    printf("\n");

    // 2. Set up the live progress display

    ProgressState progressState;
    progressState.totalPorts = totalPorts;

    bool firstDraw = true;

    // This lambda is called from worker threads after each port is scanned
    auto onPortScanned = [&](int scanned, int /*total*/, const ScanResult& result)
    {
        std::lock_guard<std::mutex> lock(progressState.mutex);

        // Update open-port count and remember the last found port description
        if (result.open)
        {
            progressState.openCount++;
            progressState.lastFoundLine = formatFoundLine(result);
        }

        // Move the cursor back up to overwrite the previous 3 lines
        if (!firstDraw)
            ansi::moveUp(3);
        firstDraw = false;

        redrawProgress(progressState, scanned, progressState.lastFoundLine);
    };

    // Draw the initial (empty) progress block before the scan starts
    {
        std::lock_guard<std::mutex> lock(progressState.mutex);
        redrawProgress(progressState, 0, "");
    }
    firstDraw = false;

    // 3. Run the scan

    std::atomic<bool> cancelFlag{ false };

    auto scanStart = std::chrono::steady_clock::now();
    std::vector<ScanResult> results = runScan(cfg, cancelFlag, onPortScanned);
    auto scanEnd   = std::chrono::steady_clock::now();

    double elapsedSeconds = std::chrono::duration<double>(scanEnd - scanStart).count();

    printf("\n\n");
    printf("  %sScan complete in %.2f seconds.%s\n\n",
           ansi::BGREEN, elapsedSeconds, ansi::RESET);

    // 4. Show and optionally save the results

    if (results.empty())
    {
        printf("  %sNo open ports found in range %d-%d.%s\n\n",
               ansi::BYELLOW, cfg.startPort, cfg.endPort, ansi::RESET);
    }
    else
    {
        printResultsTable(results);

        if (promptBool("Save results to file?", true))
            exportResultsToFile(cfg, results, elapsedSeconds);
    }

    ansi::showCursor();
    printf("  Press Enter to exit...\n");
    getchar();
    getchar();
    return 0;
}