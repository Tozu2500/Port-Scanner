#pragma once
#include <string>
#include <cstdio>
#include <windows.h>

namespace ansi {

    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD    = "\033[1m";
    constexpr const char* DIM     = "\033[2m";
 
    constexpr const char* BLACK   = "\033[30m";
    constexpr const char* RED     = "\033[31m";
    constexpr const char* GREEN   = "\033[32m";
    constexpr const char* YELLOW  = "\033[33m";
    constexpr const char* BLUE    = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN    = "\033[36m";
    constexpr const char* WHITE   = "\033[37m";
 
    constexpr const char* BBLACK   = "\033[90m";
    constexpr const char* BRED     = "\033[91m";
    constexpr const char* BGREEN   = "\033[92m";
    constexpr const char* BYELLOW  = "\033[93m";
    constexpr const char* BBLUE    = "\033[94m";
    constexpr const char* BMAGENTA = "\033[95m";
    constexpr const char* BCYAN    = "\033[96m";
    constexpr const char* BWHITE   = "\033[97m";
 
    // Cursor / screen
    constexpr const char* CLEAR_LINE  = "\033[2K\r";
    constexpr const char* HIDE_CURSOR = "\033[?25l";
    constexpr const char* SHOW_CURSOR = "\033[?25h";

    inline void clearLine() {
        printf("%s", CLEAR_LINE);
    }

    inline void hideCursor() {
        printf("%s", HIDE_CURSOR);
    }

    inline void showCursor() {
        printf("%s", SHOW_CURSOR);
    }

    inline void moveUp(int n = 1) {
        printf("\033[%dA");
    }

    inline void moveToCol(int c) {
        printf("\033[%dG");
    }
}

// Console setup
inline void enableAnsi() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    SetConsoleOutputCP(CP_UTF8);
}

inline int consoleWidth() {
    CONSOLE_SCREEN_BUFFER_INFO ci;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci))
        return ci.srWindow.Right - ci.srWindow.Left + 1;
    return 80;
}

// Progress bar
inline std::string progressBar(int done, int total, int width = 30) {
    int filled = (total > 0) ? (done * width / total) : 0;

    std::string bar;
    bar += ansi::BLUE;

    for (int i = 0; i < filled; ++i) {
        bar += (char)0xDB;
    }

    bar += ansi::DIM;
    bar += ansi::BLACK;

    for (int i = filled; i < width; ++i) {
        bar += '-';
    }

    bar += ansi::RESET;
    return bar;
}