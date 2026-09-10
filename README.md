# AeroTwin GCS

AeroTwin GCS is a Qt Quick ground control station for drone inspection workflows. It combines a QML/Qt Quick 3D operational view with native C++ telemetry, geometry, coverage, and MAVLink support.

## Features

- 3D inspection viewport with camera and frustum visualization
- Mission and asset controls
- Live telemetry HUD for attitude, heading, altitude, speed, and vertical speed
- Battery, GPS, satellite, and link-status indicators
- Coverage visualization with inspection color mapping
- UDP MAVLink telemetry ingestion
- Demo telemetry fallback when a live MAVLink connection is unavailable
- Windows UDP networking through Winsock

## Requirements

- Qt 6.7 or newer
- CMake 3.21 or newer
- A C++20 compiler
- Qt modules:
  - Qt Quick
  - Qt Quick 3D
  - Qt Quick Controls 2

The project is configured and tested with the Qt 6.8 MinGW 64-bit kit on Windows.

## Build on Windows

From a PowerShell terminal at the project root:

```powershell
cmake -S . -B build/Desktop_Qt_6_8_3_MinGW_64_bit_Debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Desktop_Qt_6_8_3_MinGW_64_bit_Debug
```

The executable is generated in the selected build directory. The exact output path can vary with the CMake generator and Qt kit.

To configure the project with Qt Creator, open the root `CMakeLists.txt`, select a Qt 6.7+ kit, configure the project, and build the `AeroTwinGCS` target.

## Run

Run the generated `AeroTwinGCS` executable from the build directory. On startup, the application listens for MAVLink telemetry over UDP port 14550.

When no live MAVLink connection is available, the interface uses the built-in demo backend so the main controls and visualization remain usable.

## Project Layout

```text
qml/                       QML application shell and reusable UI components
qml/shaders/               Qt Quick shader assets
src/                       Native C++ application and rendering code
src/telemetry/             MAVLink decoding, UDP transport, and telemetry state
CMakeLists.txt             Qt and CMake build configuration
```

## Architecture

- `src/main.cpp` creates the Qt application and loads the `AeroTwinGCS` QML module.
- `qml/Main.qml` composes the application window, viewport, mission panel, telemetry panel, and coverage dock.
- `DroneStateBridge` owns the live telemetry bridge exposed to QML.
- `GcsBackend` provides application state and demo behavior for the QML interface.
- `src/telemetry/` contains the UDP endpoint, MAVLink decoder, coordinate transforms, and ingestion core.
- The QML module includes the inspection viewport, coverage shader, HUD controls, and telemetry visualizations.

## License

All rights reserved. See the source file headers for project ownership information.
