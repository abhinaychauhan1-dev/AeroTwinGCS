/**
 * @file     : DroneStateBridge.cpp
 * @brief    : Implements the QML drone telemetry bridge.
 * @details  : Synchronizes telemetry snapshots with Qt property notifications.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "DroneStateBridge.hpp"

#include "telemetry/GeoTransform.hpp"

#include <QtCore/QDebug>
#include <QtGui/QMatrix3x3>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

using namespace aerotwin;

namespace {

constexpr qreal kAngleEpsilon = 0.01;   // degrees
constexpr qreal kMetreEpsilon = 0.005;  // metres
constexpr qreal kSpeedEpsilon = 0.01;   // m/s

/**
 * @brief Tests whether two floating-point values differ beyond a tolerance.
 * @param a First value.
 * @param b Second value.
 * @param epsilon Maximum unchanged difference.
 * @return True when the values differ by more than the tolerance.
 */
bool moved(qreal a, qreal b, qreal epsilon)
{
    return std::abs(a - b) > epsilon;
}

/**
 * @brief Converts an internal GNSS fix enum to a display label.
 * @param fix GNSS fix classification.
 * @return Human-readable fix label.
 */
QString fixName(GpsFixType fix)
{
    switch (fix) {
    case GpsFixType::RtkFixed: return QStringLiteral("RTK Fixed");
    case GpsFixType::RtkFloat: return QStringLiteral("RTK Float");
    case GpsFixType::Dgps: return QStringLiteral("DGPS");
    case GpsFixType::Fix3D: return QStringLiteral("3D Fix");
    case GpsFixType::Fix2D: return QStringLiteral("2D Fix");
    case GpsFixType::NoFix: break;
    }
    return QStringLiteral("No Fix");
}

/**
 * @brief Converts a telemetry-source enum to a display label.
 * @param source Source that supplied the vehicle state.
 * @return Human-readable source label.
 */
QString sourceLabel(TelemetrySource source)
{
    switch (source) {
    case TelemetrySource::Mavlink: return QStringLiteral("MAVLink");
    case TelemetrySource::Ros2: return QStringLiteral("ROS 2");
    case TelemetrySource::Zenoh: return QStringLiteral("Zenoh");
    case TelemetrySource::Simulated: return QStringLiteral("Simulated");
    case TelemetrySource::Unknown: break;
    }
    return QStringLiteral("Offline");
}

/**
 * @brief Returns the current monotonic timestamp in microseconds.
 * @return Microseconds from an unspecified steady-clock epoch.
 */
std::uint64_t monotonicUs()
{
    using namespace std::chrono;
    return std::uint64_t(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace

/**
 * @brief Creates the bridge and starts its fixed-rate GUI update timer.
 * @param parent Optional Qt object owner.
 */
DroneStateBridge::DroneStateBridge(QObject *parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(1000 / m_updateRateHz);
    connect(&m_timer, &QTimer::timeout, this, &DroneStateBridge::tick);
    m_timer.start();
}

/** @brief Stops telemetry ingestion before the bridge is destroyed. */
DroneStateBridge::~DroneStateBridge()
{
    m_core.stop();
}

/**
 * @brief Starts MAVLink telemetry ingestion on a UDP port.
 * @param udpPort UDP port expected to receive MAVLink datagrams.
 * @return True when ingestion starts successfully.
 */
bool DroneStateBridge::startMavlink(int udpPort)
{
    std::string error;
    if (!m_core.start(std::uint16_t(udpPort), &error)) {
        qWarning("DroneStateBridge: MAVLink ingest failed to start: %s", error.c_str());
        return false;
    }

    if (!m_running) {
        m_running = true;
        Q_EMIT runningChanged();
    }
    return true;
}

/** @brief Stops telemetry ingestion and clears the live connection state. */
void DroneStateBridge::stop()
{
    m_core.stop();
    if (m_running) {
        m_running = false;
        Q_EMIT runningChanged();
    }
    if (m_connected) {
        m_connected = false;
        Q_EMIT connectedChanged();
    }
}

/**
 * @brief Sets a manual global origin for ENU coordinate conversion.
 * @param latitudeDeg Origin latitude in degrees.
 * @param longitudeDeg Origin longitude in degrees.
 * @param altitudeAmsl Origin altitude above mean sea level in metres.
 */
void DroneStateBridge::setOrigin(double latitudeDeg, double longitudeDeg, double altitudeAmsl)
{
    m_core.setAutoOrigin(false);
    m_core.setOrigin({latitudeDeg, longitudeDeg, altitudeAmsl});
    Q_EMIT originChanged();
}

/**
 * @brief Uses the latest valid GNSS position as the ENU origin.
 * @return True when a global fix was available to latch.
 */
bool DroneStateBridge::latchOriginHere()
{
    const VehicleState state = m_core.snapshot();
    if (!state.hasGlobalFix)
        return false;

    m_core.setAutoOrigin(false);
    m_core.setOrigin(state.global);
    Q_EMIT originChanged();
    return true;
}

/** @brief Restores automatic ENU-origin selection from valid GNSS fixes. */
void DroneStateBridge::resetOrigin()
{
    m_core.setAutoOrigin(true);
    Q_EMIT originChanged();
}

/**
 * @brief Sets the scale factor between ENU metres and scene units.
 * @param scale Positive scene-unit scale.
 */
void DroneStateBridge::setSceneScale(qreal scale)
{
    const qreal clamped = std::max(qreal(1e-6), scale);
    if (!moved(m_sceneScale, clamped, 1e-9))
        return;
    m_sceneScale = clamped;
    Q_EMIT sceneScaleChanged();
    Q_EMIT localPositionChanged();
}

/**
 * @brief Sets the maximum GUI publication rate for telemetry.
 * @param hz Requested update frequency, clamped to the supported range.
 */
void DroneStateBridge::setUpdateRateHz(int hz)
{
    const int clamped = std::clamp(hz, 1, 240);
    if (clamped == m_updateRateHz)
        return;
    m_updateRateHz = clamped;
    m_timer.setInterval(1000 / m_updateRateHz);
    Q_EMIT updateRateHzChanged();
}

/**
 * @brief Sets the elapsed-packet limit used for live-link detection.
 * @param ms Timeout in milliseconds, clamped to a safe minimum.
 */
void DroneStateBridge::setLinkTimeoutMs(int ms)
{
    const int clamped = std::max(50, ms);
    if (clamped == m_linkTimeoutMs)
        return;
    m_linkTimeoutMs = clamped;
    Q_EMIT linkTimeoutMsChanged();
}

/** @brief Publishes changed telemetry fields to QML at the configured GUI rate. */
void DroneStateBridge::tick()
{
    // Link health is evaluated every tick even when no new frame arrived,
    // otherwise a dead link would keep reporting the last good state forever.
    const std::uint64_t last = m_core.lastPacketMonotonicUs();
    const bool alive = last != 0
        && (monotonicUs() - last) < std::uint64_t(m_linkTimeoutMs) * 1000ull;
    if (alive != m_connected) {
        m_connected = alive;
        Q_EMIT connectedChanged();
    }

    const qulonglong datagrams = m_core.datagramsReceived();
    const qulonglong failures = m_core.decodeFailures();
    if (datagrams != m_datagrams || failures != m_decodeFailures) {
        m_datagrams = datagrams;
        m_decodeFailures = failures;
        Q_EMIT statisticsChanged();
    }

    // Wait-free read; skipped entirely when the producer has not moved on.
    const std::uint32_t generation = m_core.generation();
    if (generation == m_lastGeneration)
        return;
    m_lastGeneration = generation;

    applySnapshot(m_core.snapshot());
    Q_EMIT frameUpdated();
}

/**
 * @brief Compares an ingest snapshot with published fields and emits changes.
 * @param state Latest lock-free vehicle state snapshot.
 */
void DroneStateBridge::applySnapshot(const VehicleState &state)
{
    if (const QString label = sourceLabel(state.source); label != m_sourceName) {
        m_sourceName = label;
        Q_EMIT sourceNameChanged();
    }

    if (state.armed != m_armed) {
        m_armed = state.armed;
        Q_EMIT armedChanged();
    }

    const int battery = int(std::lround(state.batteryPercent));
    if (battery != m_batteryPercent || moved(state.batteryVolts, m_batteryVolts, 0.05)) {
        m_batteryPercent = battery;
        m_batteryVolts = state.batteryVolts;
        Q_EMIT batteryChanged();
    }

    const QString fix = fixName(state.fix);
    if (fix != m_gpsFix || int(state.satellites) != m_satellites) {
        m_gpsFix = fix;
        m_satellites = int(state.satellites);
        Q_EMIT gnssChanged();
    }

    if (int(state.linkLatencyMs) != m_linkLatencyMs) {
        m_linkLatencyMs = int(state.linkLatencyMs);
        Q_EMIT linkChanged();
    }

    if (state.hasGlobalFix
        && (moved(state.global.latitudeDeg, m_latitude, 1e-8)
            || moved(state.global.longitudeDeg, m_longitude, 1e-8)
            || moved(state.global.altitudeAmsl, m_altitudeAmsl, kMetreEpsilon))) {
        m_latitude = state.global.latitudeDeg;
        m_longitude = state.global.longitudeDeg;
        m_altitudeAmsl = state.global.altitudeAmsl;
        Q_EMIT globalPositionChanged();
    }

    // WGS-84 -> ENU metres -> Qt Quick 3D scene units.
    const Vec3 enu = m_core.enuPosition(state);
    const QVector3D enuVector(float(enu.x), float(enu.y), float(enu.z));
    if ((enuVector - m_enu).lengthSquared() > float(kMetreEpsilon * kMetreEpsilon)) {
        m_enu = enuVector;
        const Vec3 scene = GeoTransform::enuToScene(enu, m_sceneScale);
        m_scenePosition = QVector3D(float(scene.x), float(scene.y), float(scene.z));
        Q_EMIT localPositionChanged();
    }

    const EulerAngles euler = GeoTransform::quaternionToEuler(state.orientation);
    if (moved(euler.rollDeg, m_roll, kAngleEpsilon)
        || moved(euler.pitchDeg, m_pitch, kAngleEpsilon)
        || moved(euler.yawDeg, m_yaw, kAngleEpsilon)) {
        m_roll = euler.rollDeg;
        m_pitch = euler.pitchDeg;
        m_yaw = euler.yawDeg;

        // Qt needs a scene-space rotation, not the aircraft's native NED frame.
        const std::array<float, 9> r = GeoTransform::bodyToSceneMatrix(state.orientation);
        QMatrix3x3 matrix;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                matrix(row, col) = r[std::size_t(row * 3 + col)];
        m_sceneRotation = QQuaternion::fromRotationMatrix(matrix);

        Q_EMIT attitudeChanged();
    }

    const qreal ground = std::hypot(state.velocityNed.x, state.velocityNed.y);
    const qreal vertical = -state.velocityNed.z; // NED down -> climb positive
    if (moved(ground, m_groundSpeed, kSpeedEpsilon) || moved(vertical, m_verticalSpeed, kSpeedEpsilon)) {
        m_groundSpeed = ground;
        m_verticalSpeed = vertical;
        Q_EMIT velocityChanged();
    }
}
