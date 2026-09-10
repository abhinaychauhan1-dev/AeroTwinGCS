/**
 * @file     : DroneStateBridge.hpp
 * @brief    : Bridges thread-safe drone telemetry into QML properties.
 * @details  : Publishes throttled vehicle state for the user interface.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include "telemetry/TelemetryIngestCore.hpp"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QQuaternion>
#include <QtGui/QVector3D>
#include <QtQml/qqmlregistration.h>

/*!
    \class DroneStateBridge

    QML singleton that turns the lock-free telemetry snapshot into throttled Qt
    property notifications.

    The ingest side runs at whatever rate the vehicle emits (MAVLink at
    50-250 Hz, ROS 2 odometry often faster). Emitting a Qt signal per packet
    would drown the GUI thread in metacalls and stall the render loop, so this
    bridge polls the seqlock at a fixed cadence - 60 Hz by default, matched to
    the compositor - and emits only the properties that actually moved.

    Wiring a non-MAVLink transport:

        auto &core = DroneStateBridge::instance()->core();
        // rclcpp subscription callback, Zenoh handler, ...
        core.publish(stateFromRosMsg(*msg));
*/
class DroneStateBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged FINAL)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged FINAL)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY sourceNameChanged FINAL)

    Q_PROPERTY(bool armed READ armed NOTIFY armedChanged FINAL)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryChanged FINAL)
    Q_PROPERTY(qreal batteryVolts READ batteryVolts NOTIFY batteryChanged FINAL)
    Q_PROPERTY(QString gpsFix READ gpsFix NOTIFY gnssChanged FINAL)
    Q_PROPERTY(int satellites READ satellites NOTIFY gnssChanged FINAL)
    Q_PROPERTY(int linkLatencyMs READ linkLatencyMs NOTIFY linkChanged FINAL)

    Q_PROPERTY(double latitude READ latitude NOTIFY globalPositionChanged FINAL)
    Q_PROPERTY(double longitude READ longitude NOTIFY globalPositionChanged FINAL)
    Q_PROPERTY(double altitudeAmsl READ altitudeAmsl NOTIFY globalPositionChanged FINAL)
    Q_PROPERTY(qreal altitudeAboveOrigin READ altitudeAboveOrigin NOTIFY localPositionChanged FINAL)

    Q_PROPERTY(QVector3D enuPosition READ enuPosition NOTIFY localPositionChanged FINAL)
    Q_PROPERTY(QVector3D scenePosition READ scenePosition NOTIFY localPositionChanged FINAL)
    Q_PROPERTY(QQuaternion sceneRotation READ sceneRotation NOTIFY attitudeChanged FINAL)

    Q_PROPERTY(qreal roll READ roll NOTIFY attitudeChanged FINAL)
    Q_PROPERTY(qreal pitch READ pitch NOTIFY attitudeChanged FINAL)
    Q_PROPERTY(qreal yaw READ yaw NOTIFY attitudeChanged FINAL)

    Q_PROPERTY(qreal groundSpeed READ groundSpeed NOTIFY velocityChanged FINAL)
    Q_PROPERTY(qreal verticalSpeed READ verticalSpeed NOTIFY velocityChanged FINAL)

    Q_PROPERTY(qreal sceneScale READ sceneScale WRITE setSceneScale NOTIFY sceneScaleChanged FINAL)
    Q_PROPERTY(int updateRateHz READ updateRateHz WRITE setUpdateRateHz NOTIFY updateRateHzChanged FINAL)
    Q_PROPERTY(int linkTimeoutMs READ linkTimeoutMs WRITE setLinkTimeoutMs NOTIFY linkTimeoutMsChanged FINAL)

    Q_PROPERTY(qulonglong datagramsReceived READ datagramsReceived NOTIFY statisticsChanged FINAL)
    Q_PROPERTY(qulonglong decodeFailures READ decodeFailures NOTIFY statisticsChanged FINAL)

public:
    /** @brief Creates the QML telemetry bridge. @param parent Optional Qt object owner. */
    explicit DroneStateBridge(QObject *parent = nullptr);
    /** @brief Stops telemetry ingestion before object destruction. */
    ~DroneStateBridge() override;

    /*! Non-QML access for ROS 2 / Zenoh adapters. */
    [[nodiscard]] aerotwin::TelemetryIngestCore &core() noexcept { return m_core; }

    /** @brief Returns link liveness derived from recent packets. @return True when connected. */
    [[nodiscard]] bool connected() const noexcept { return m_connected; }
    [[nodiscard]] bool running() const noexcept { return m_running; }
    [[nodiscard]] QString sourceName() const { return m_sourceName; }

    [[nodiscard]] bool armed() const noexcept { return m_armed; }
    [[nodiscard]] int batteryPercent() const noexcept { return m_batteryPercent; }
    [[nodiscard]] qreal batteryVolts() const noexcept { return m_batteryVolts; }
    [[nodiscard]] QString gpsFix() const { return m_gpsFix; }
    [[nodiscard]] int satellites() const noexcept { return m_satellites; }
    [[nodiscard]] int linkLatencyMs() const noexcept { return m_linkLatencyMs; }

    [[nodiscard]] double latitude() const noexcept { return m_latitude; }
    [[nodiscard]] double longitude() const noexcept { return m_longitude; }
    [[nodiscard]] double altitudeAmsl() const noexcept { return m_altitudeAmsl; }
    [[nodiscard]] qreal altitudeAboveOrigin() const noexcept { return m_enu.z(); }

    [[nodiscard]] QVector3D enuPosition() const noexcept { return m_enu; }
    [[nodiscard]] QVector3D scenePosition() const noexcept { return m_scenePosition; }
    [[nodiscard]] QQuaternion sceneRotation() const noexcept { return m_sceneRotation; }

    [[nodiscard]] qreal roll() const noexcept { return m_roll; }
    [[nodiscard]] qreal pitch() const noexcept { return m_pitch; }
    [[nodiscard]] qreal yaw() const noexcept { return m_yaw; }

    [[nodiscard]] qreal groundSpeed() const noexcept { return m_groundSpeed; }
    [[nodiscard]] qreal verticalSpeed() const noexcept { return m_verticalSpeed; }

    [[nodiscard]] qreal sceneScale() const noexcept { return m_sceneScale; }
    /** @brief Sets ENU-to-scene scaling. @param scale Positive scale factor. */
    void setSceneScale(qreal scale);

    [[nodiscard]] int updateRateHz() const noexcept { return m_updateRateHz; }
    /** @brief Sets GUI telemetry update frequency. @param hz Requested frequency in hertz. */
    void setUpdateRateHz(int hz);

    [[nodiscard]] int linkTimeoutMs() const noexcept { return m_linkTimeoutMs; }
    /** @brief Sets link-loss timeout. @param ms Timeout in milliseconds. */
    void setLinkTimeoutMs(int ms);

    [[nodiscard]] qulonglong datagramsReceived() const noexcept { return m_datagrams; }
    [[nodiscard]] qulonglong decodeFailures() const noexcept { return m_decodeFailures; }

    /** @brief Starts MAVLink UDP ingestion. @param udpPort Local listening port. @return True on success. */
    Q_INVOKABLE bool startMavlink(int udpPort = 14550);
    /** @brief Stops MAVLink ingestion. */
    Q_INVOKABLE void stop();
    /** @brief Sets a manual ENU origin. @param latitudeDeg Latitude. @param longitudeDeg Longitude. @param altitudeAmsl Altitude AMSL. */
    Q_INVOKABLE void setOrigin(double latitudeDeg, double longitudeDeg, double altitudeAmsl);
    /** @brief Latches the latest GNSS position as origin. @return True when a fix is available. */
    Q_INVOKABLE bool latchOriginHere();
    /** @brief Restores automatic origin selection. */
    Q_INVOKABLE void resetOrigin();

Q_SIGNALS:
    void connectedChanged();
    void runningChanged();
    void sourceNameChanged();
    void armedChanged();
    void batteryChanged();
    void gnssChanged();
    void linkChanged();
    void globalPositionChanged();
    void localPositionChanged();
    void attitudeChanged();
    void velocityChanged();
    void sceneScaleChanged();
    void updateRateHzChanged();
    void linkTimeoutMsChanged();
    void statisticsChanged();
    void originChanged();

    /*! One pulse per throttled tick, for QML that wants a single hook. */
    void frameUpdated();

private:
    /** @brief Polls a lock-free state snapshot and emits changed QML properties. */
    void tick();
    /** @brief Applies one telemetry snapshot. @param state Latest vehicle state. */
    void applySnapshot(const aerotwin::VehicleState &state);

    aerotwin::TelemetryIngestCore m_core;
    QTimer m_timer;
    std::uint32_t m_lastGeneration = 0;

    bool m_connected = false;
    bool m_running = false;
    QString m_sourceName = QStringLiteral("Offline");

    bool m_armed = false;
    int m_batteryPercent = 0;
    qreal m_batteryVolts = 0.0;
    QString m_gpsFix = QStringLiteral("No Fix");
    int m_satellites = 0;
    int m_linkLatencyMs = 0;

    double m_latitude = 0.0;
    double m_longitude = 0.0;
    double m_altitudeAmsl = 0.0;

    QVector3D m_enu;
    QVector3D m_scenePosition;
    QQuaternion m_sceneRotation;

    qreal m_roll = 0.0;
    qreal m_pitch = 0.0;
    qreal m_yaw = 0.0;
    qreal m_groundSpeed = 0.0;
    qreal m_verticalSpeed = 0.0;

    qreal m_sceneScale = 1.0;
    int m_updateRateHz = 60;
    int m_linkTimeoutMs = 1500;

    qulonglong m_datagrams = 0;
    qulonglong m_decodeFailures = 0;
};
