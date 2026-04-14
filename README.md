# Port-Scanner
A multithreaded TCP port scanner written in C++. Supports banner grabbing, configurable timeouts, and thread counts. Results can be saved to a file.

---

## Prerequisites

- Windows 10 or later
- [Visual Studio 2019+](https://visualstudio.microsoft.com/) with the **Desktop development with C++** workload, **or** the standalone MSVC build tools
- Winsock2 (included with the Windows SDK — no separate install needed)

---

## Building

### Visual Studio IDE
1. Open the `port_scanner/` folder or create a new project from existing files.
2. Add all `.cpp` files (`main.cpp`, `scanner.cpp`) to the project.
3. Set the configuration to **Release** and platform to **x64**.
4. Build the solution (**Ctrl+Shift+B**).
5. The output executable will be at `port_scanner\port_scanner.exe` (or inside a `Release\` subfolder depending on your project settings).

### Command Line (MSVC)
Open a **Developer Command Prompt for VS** and run:

```bat
cd port_scanner
cl /EHsc /O2 /std:c++17 main.cpp scanner.cpp /Fe:port_scanner.exe /link ws2_32.lib
```

---

## Usage

Run the executable directly or use the provided launcher (see below). On startup, the scanner will prompt you for:

| Setting | Default | Description |
|---|---|---|
| Host | — | Hostname or IP address to scan |
| Start port | 1 | First port in the scan range |
| End port | 1024 | Last port in the scan range |
| Threads | 100 | Number of concurrent worker threads |
| Timeout | 1500 ms | TCP connect timeout per port |
| Grab banners | yes | Read service banners from open ports |
| Save results | — | Optional filename to write the report to |

Press **Ctrl+C** at any time to cancel a scan in progress.

---

## Launcher

A `launch.bat` file is included in the root folder for convenience. It automatically locates `port_scanner\port_scanner.exe` relative to its own location, so it works regardless of where you run it from.

```bat
launch.bat
```

If the executable is not found, the launcher will print a clear error message and pause so you can read it before the window closes.
