# /**
#  * @file     : CMakeLists.txt
#  * @brief    : Configures the AeroTwin Ground Control Station build.
#  * @details  : Defines the Qt executable, QML module, and source dependencies.
#  * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
#  * @version  : 1.0.0
#  *
#  * Copyright (c) 2024
#  * All rights reserved.
#  */

# Require modern CMake features used by the Qt 6 project setup helpers.
cmake_minimum_required(VERSION 3.21)

# Define the C++ project name, version, and enabled language.
project(AeroTwinGCS VERSION 1.0 LANGUAGES CXX)

# Compile production code as C++20 and generate Qt meta-object code automatically.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Locate the Qt Quick, 3D scene, and control modules required by the application.
find_package(Qt6 6.7 REQUIRED COMPONENTS Quick Quick3D QuickControls2)

# Apply Qt's standard target, deployment, and QML build configuration.
qt_standard_project_setup(REQUIRES 6.7)

# Build the native executable, including telemetry, geometry, and coverage sources.
qt_add_executable(AeroTwinGCS
    src/main.cpp
    src/GcsBackend.h
    src/DronePointCloudGeometry.hpp
    src/DronePointCloudGeometry.cpp
    src/FrustumGeometry.hpp
    src/FrustumGeometry.cpp
    src/CoverageMapTextureData.hpp
    src/CoverageMapTextureData.cpp
    src/InspectionColorMap.hpp
    src/InspectionColorMap.cpp
    src/DroneStateBridge.hpp
    src/DroneStateBridge.cpp
    src/telemetry/TelemetryTypes.hpp
    src/telemetry/SeqLock.hpp
    src/telemetry/GeoTransform.hpp
    src/telemetry/GeoTransform.cpp
    src/telemetry/MavlinkDecoder.hpp
    src/telemetry/MavlinkDecoder.cpp
    src/telemetry/UdpTelemetryEndpoint.hpp
    src/telemetry/UdpTelemetryEndpoint.cpp
    src/telemetry/TelemetryIngestCore.hpp
    src/telemetry/TelemetryIngestCore.cpp
)

# Register QML singleton files before adding them to the application module.
set(AEROTWIN_QML_SINGLETONS
    qml/Theme.qml
)
set_source_files_properties(${AEROTWIN_QML_SINGLETONS}
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Bundle the QML interface and shader asset into the AeroTwinGCS QML module.
qt_add_qml_module(AeroTwinGCS
    URI AeroTwinGCS
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/GlassPanel.qml
        qml/HudButton.qml
        qml/StatusChip.qml
        qml/SectionLabel.qml
        qml/Viewport3D.qml
        qml/CameraFrustum.qml
        qml/CoverageMaterial.qml
        qml/InspectionCameraController.qml
        qml/HeaderBar.qml
        qml/MissionPanel.qml
        qml/TelemetryPanel.qml
        qml/CoverageDock.qml
        qml/AttitudeIndicator.qml
        qml/AltitudeTape.qml
        qml/HeadingRibbon.qml
        qml/VerticalSpeedIndicator.qml
    RESOURCES
        qml/shaders/coverage.frag
)

# Allow native sources to include project headers using the src root.
target_include_directories(AeroTwinGCS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link the Qt modules that provide the QML UI, controls, and 3D renderer.
target_link_libraries(AeroTwinGCS PRIVATE
    Qt6::Quick
    Qt6::Quick3D
    Qt6::QuickControls2
)

# Link Winsock for the Windows UDP telemetry endpoint.
if(WIN32)
    target_link_libraries(AeroTwinGCS PRIVATE ws2_32)
endif()

# Define installation destinations for desktop application deployment.
install(TARGETS AeroTwinGCS
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
