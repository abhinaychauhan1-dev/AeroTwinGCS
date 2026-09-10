/**
 * @file     : GcsBackend.h
 * @brief    : Demo backend for ground control state and mission actions.
 * @details  : Exposes vehicle, telemetry, and inspection state to QML.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QTimer>
#include <QtMath>

/*!
    Single point of contact between the QML shell and the MAVLink / inspection
    stack. Every Q_PROPERTY here has a matching alias in Main.qml, so a real
    backend can either replace this singleton or drive the root object directly:

        engine.rootObjects().first()->setProperty("batteryPercent", 74);
*/
class GcsBackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // --- Vehicle state -----------------------------------------------------
    Q_PROPERTY(bool armed MEMBER m_armed NOTIFY changed)
    Q_PROPERTY(QString flightMode MEMBER m_flightMode NOTIFY changed)
    Q_PROPERTY(int batteryPercent MEMBER m_batteryPercent NOTIFY changed)
    Q_PROPERTY(qreal batteryVolts MEMBER m_batteryVolts NOTIFY changed)
    Q_PROPERTY(QString gpsFix MEMBER m_gpsFix NOTIFY changed)          // "RTK Fixed" | "RTK Float" | "3D" | "No Fix"
    Q_PROPERTY(int satellites MEMBER m_satellites NOTIFY changed)
    Q_PROPERTY(int linkLatencyMs MEMBER m_linkLatencyMs NOTIFY changed)
    Q_PROPERTY(bool mavlinkAlive MEMBER m_mavlinkAlive NOTIFY changed)

    // --- Telemetry ---------------------------------------------------------
    Q_PROPERTY(qreal pitch MEMBER m_pitch NOTIFY changed)
    Q_PROPERTY(qreal roll MEMBER m_roll NOTIFY changed)
    Q_PROPERTY(qreal heading MEMBER m_heading NOTIFY changed)
    Q_PROPERTY(qreal altitudeAgl MEMBER m_altitudeAgl NOTIFY changed)
    Q_PROPERTY(qreal verticalSpeed MEMBER m_verticalSpeed NOTIFY changed)
    Q_PROPERTY(qreal groundSpeed MEMBER m_groundSpeed NOTIFY changed)

    // --- Inspection coverage ----------------------------------------------
    Q_PROPERTY(QString activeAsset MEMBER m_activeAsset NOTIFY changed)
    Q_PROPERTY(qreal coverage MEMBER m_coverage NOTIFY changed)        // 0.0 - 1.0
    Q_PROPERTY(int hotspotCount MEMBER m_hotspotCount NOTIFY changed)
    Q_PROPERTY(bool rerouting MEMBER m_rerouting NOTIFY changed)

public:
    /** @brief Creates the demo backend and starts simulated telemetry updates. @param parent Optional Qt object owner. */
    explicit GcsBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
        // Demo pulse so the shell is alive without a vehicle attached.
        connect(&m_sim, &QTimer::timeout, this, &GcsBackend::tick);
        m_sim.start(50);
    }

    /** @brief Changes the simulated arming state. @param value True to arm the vehicle. */
    Q_INVOKABLE void setArmed(bool value)
    {
        m_armed = value;
        emit changed();
    }

    /** @brief Selects the asset currently being inspected. @param asset Asset display name. */
    Q_INVOKABLE void selectAsset(const QString &asset)
    {
        m_activeAsset = asset;
        emit changed();
        emit assetSelected(asset);
    }

    /** @brief Forwards a mission-editing command to the integration layer. @param command Requested waypoint action. */
    Q_INVOKABLE void sendWaypointCommand(const QString &command)
    {
        emit waypointCommandRequested(command);
    }

    /** @brief Starts a simulated reroute for the remaining hotspot zones. */
    Q_INVOKABLE void autoRerouteMissingZones()
    {
        if (m_rerouting || m_hotspotCount <= 0)
            return;

        const int resolvedZones = qMin(3, m_hotspotCount);
        m_rerouting = true;
        emit changed();
        emit rerouteRequested();

        QTimer::singleShot(2500, this, [this, resolvedZones] {
            m_rerouting = false;
            m_hotspotCount -= resolvedZones;
            m_coverage = qMin(1.0, m_coverage + 0.04 * resolvedZones);
            emit changed();
        });
    }

signals:
    /** @brief Emitted after any exposed demo state changes. */
    void changed();
    /** @brief Emitted when the active inspection asset changes. @param asset Selected asset name. */
    void assetSelected(const QString &asset);
    /** @brief Emitted when a waypoint command is requested. @param command Requested action. */
    void waypointCommandRequested(const QString &command);
    /** @brief Emitted when automatic rerouting begins. */
    void rerouteRequested();

private:
    /** @brief Advances the demonstration telemetry and coverage state. */
    void tick()
    {
        m_phase += 0.05;
        m_pitch = 7.0 * qSin(m_phase * 0.7);
        m_roll = 14.0 * qSin(m_phase * 0.45);
        m_heading = qreal(int(m_heading + 0.25) % 360);
        m_verticalSpeed = 1.4 * qSin(m_phase * 0.9);
        m_altitudeAgl = qMax(0.0, m_altitudeAgl + m_verticalSpeed * 0.05);
        m_groundSpeed = 4.5 + 0.6 * qSin(m_phase * 0.3);
        m_linkLatencyMs = 18 + QRandomGenerator::global()->bounded(9);
        if (m_armed && m_coverage < 1.0)
            m_coverage = qMin(1.0, m_coverage + 0.0004);
        emit changed();
    }

    QTimer m_sim;
    qreal m_phase = 0.0;

    bool m_armed = false;
    QString m_flightMode = QStringLiteral("INSPECT");
    int m_batteryPercent = 78;
    qreal m_batteryVolts = 23.4;
    QString m_gpsFix = QStringLiteral("RTK Fixed");
    int m_satellites = 21;
    int m_linkLatencyMs = 22;
    bool m_mavlinkAlive = true;

    qreal m_pitch = 0.0;
    qreal m_roll = 0.0;
    qreal m_heading = 42.0;
    qreal m_altitudeAgl = 34.5;
    qreal m_verticalSpeed = 0.0;
    qreal m_groundSpeed = 4.8;

    QString m_activeAsset = QStringLiteral("Bridge Truss A");
    qreal m_coverage = 0.62;
    int m_hotspotCount = 7;
    bool m_rerouting = false;
};
