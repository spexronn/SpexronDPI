# SpexronDPI

SpexronDPI is a lightweight, Windows-based Deep Packet Inspection (DPI) engine and network filtering tool. It uses [WinDivert](https://reqrypt.org/windivert.html) to intercept, analyze, and modify network traffic on the fly at the packet level.

Whether you're looking to bypass certain network restrictions, analyze Layer 7 payloads, or just monitor what's going in and out of your machine, this tool provides a low-overhead solution with a clean graphical interface.

## What's inside?

- **Real-time Packet Capture**: Hooks directly into the Windows network stack via WinDivert.
- **Layer 7 Parsing**: Inspects deep into application-layer payloads.
- **Rule Engine**: Define custom rules to drop, modify, or allow specific packets.
- **Live Dashboard**: Built with ImGui, giving you real-time telemetry and statistics without eating up system resources.
- **Standalone**: Everything runs locally. No sketchy remote servers.

## Installation

You don't need to build it yourself if you just want to use it. 
Check the [Releases](../../releases/latest) page and download the `SpexronDPI-Setup.exe` installer. 

*Note: Since it intercepts network traffic, it requires administrator privileges to run. Windows might throw a SmartScreen warning because it's a new, unsigned executable—just click "Run anyway".*

## Building from source

If you want to poke around the code or compile it yourself, you'll need:
- Visual Studio (MSVC)
- CMake
- Windows SDK

1. Clone the repo:
   ```bash
   git clone https://github.com/spexronn/SpexronDPI.git
   cd SpexronDPI
   ```
2. Configure and build:
   ```bash
   cmake -B build
   cmake --build build --config Release
   ```
3. The compiled executable will be waiting for you in `build/bin/Release/`. 

*Make sure to copy the WinDivert driver files (`WinDivert.dll` and `WinDivert64.sys`) into the same directory as the executable before running it, or it will crash.*
