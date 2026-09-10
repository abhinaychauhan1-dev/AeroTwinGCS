/**
 * @file     : TelemetryTypes.hpp
 * @brief    : Defines common vehicle telemetry data types.
 * @details  : Provides the state structures exchanged by telemetry components.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include <cstdint>
#include <type_traits>

namespace aerotwin {

/** @brief Identifies the subsystem that supplied a vehicle state. */
enum class TelemetrySource : std::uint8_t {
    Unknown = 0,
    Mavlink,
    Ros2,
    Zenoh,
    Simulated,
};

/** @brief Describes the quality of the vehicle's GNSS position fix. */
enum class GpsFixType : std::uint8_t {
    NoFix = 0,
    Fix2D,
    Fix3D,
    Dgps,
    RtkFloat,
    RtkFixed,
};

/** @brief Stores a WGS-84 geographic point. */
struct GeoPoint
{
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeAmsl = 0.0; // metres above mean sea level
};

/** @brief Stores a three-dimensional Cartesian vector. */
struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/*! Hamilton quaternion, body -> reference frame, scalar first. */
struct Quat
{
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/** @brief Stores aircraft roll, pitch, and yaw angles in degrees. */
struct EulerAngles
{
    double rollDeg = 0.0;
    double pitchDeg = 0.0;
    double yawDeg = 0.0;
};

/*!
    One coherent vehicle snapshot.

    Deliberately trivially copyable and free of Qt types so it can live in a
    seqlock slot and be published from a MAVLink jthread, a rclcpp executor
    callback or a Zenoh subscriber without any allocation or ownership dance.
*/
struct VehicleState
{
    std::uint64_t timestampUs = 0;
    TelemetrySource source = TelemetrySource::Unknown;

    GeoPoint global{};
    bool hasGlobalFix = false;

    /*! Local odometry in NED metres; used when no global fix is available. */
    Vec3 positionNed{};
    bool hasLocalOdometry = false;

    Vec3 velocityNed{};        // m/s
    Quat orientation{};        // body -> NED

    GpsFixType fix = GpsFixType::NoFix;
    std::uint8_t satellites = 0;
    std::uint16_t linkLatencyMs = 0;

    float batteryPercent = 0.0f;
    float batteryVolts = 0.0f;

    bool armed = false;
    std::uint8_t systemId = 0;
    std::uint32_t customMode = 0;
};

static_assert(std::is_trivially_copyable_v<VehicleState>,
              "VehicleState is published through a seqlock and must be memcpy-able.");

} // namespace aerotwin
