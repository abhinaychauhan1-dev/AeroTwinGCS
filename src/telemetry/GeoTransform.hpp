/**
 * @file     : GeoTransform.hpp
 * @brief    : Declares geographic coordinate transformation utilities.
 * @details  : Converts global coordinates into local ENU scene coordinates.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include "TelemetryTypes.hpp"

#include <array>

namespace aerotwin {

/*!
    WGS-84 / NED / ENU conversions plus attitude decomposition.

    Frames used across the stack:
      * WGS-84  geodetic latitude, longitude, altitude AMSL (MAVLink, GNSS)
      * NED     local tangent plane, x=North y=East z=Down (MAVLink, PX4, ROS 2
                aerospace convention)
      * ENU     local tangent plane, x=East y=North z=Up (ROS REP-103)
      * Scene   Qt Quick 3D, right-handed and Y-up: x=East y=Up z=-North

    All Qt Quick 3D content is authored Y-up with -Z forward, so North maps to
    -Z and the scene basis stays right-handed with no mirroring - a mirrored
    basis would silently flip winding order and break backface culling on
    imported CAD assets.
*/
class GeoTransform
{
public:
    // WGS-84 ellipsoid
    static constexpr double SemiMajorAxis = 6378137.0;
    static constexpr double Flattening = 1.0 / 298.257223563;
    static constexpr double EccentricitySquared = Flattening * (2.0 - Flattening);

    /*! Pins the local tangent plane. Usually the first 3D fix or the home point. */
    void setOrigin(const GeoPoint &origin) noexcept;
    /** @brief Returns origin availability. @return True when an origin is pinned. */
    [[nodiscard]] bool hasOrigin() const noexcept { return m_hasOrigin; }
    /** @brief Returns the pinned geographic origin. @return Origin coordinates. */
    [[nodiscard]] GeoPoint origin() const noexcept { return m_origin; }
    /** @brief Clears the pinned origin. */
    void resetOrigin() noexcept { m_hasOrigin = false; }

    /*! Geodetic -> local ENU metres relative to the pinned origin. */
    [[nodiscard]] Vec3 geodeticToEnu(const GeoPoint &point) const noexcept;

    /*! Geodetic -> earth-centred earth-fixed metres. */
    [[nodiscard]] static Vec3 geodeticToEcef(const GeoPoint &point) noexcept;

    /** @brief Converts NED coordinates to ENU. @param ned NED vector. @return ENU vector. */
    [[nodiscard]] static constexpr Vec3 nedToEnu(const Vec3 &ned) noexcept
    {
        return {ned.y, ned.x, -ned.z};
    }

    /** @brief Converts ENU metres to Qt scene coordinates. @param enu ENU vector. @param scale Scene scale. @return Scene vector. */
    [[nodiscard]] static constexpr Vec3 enuToScene(const Vec3 &enu, double scale) noexcept
    {
        return {enu.x * scale, enu.z * scale, -enu.y * scale};
    }

    /*!
        Hamilton quaternion (body -> NED) to aerospace Euler angles using the
        ZYX / yaw-pitch-roll sequence, degrees. Pitch is clamped before asin so
        a marginally denormalised quaternion cannot produce NaN near +/-90.
    */
    [[nodiscard]] static EulerAngles quaternionToEuler(const Quat &q) noexcept;

    [[nodiscard]] static Quat normalize(const Quat &q) noexcept;

    /*!
        Rotation matrix of the body frame expressed in the Qt Quick 3D scene
        basis: R_scene = M * R_ned_body * M^T, with M the NED -> scene change of
        basis. Returned row-major so it can be handed to
        QQuaternion::fromRotationMatrix without further juggling.
    */
    [[nodiscard]] static std::array<float, 9> bodyToSceneMatrix(const Quat &q) noexcept;

private:
    GeoPoint m_origin{};
    Vec3 m_originEcef{};
    double m_sinLat = 0.0;
    double m_cosLat = 1.0;
    double m_sinLon = 0.0;
    double m_cosLon = 1.0;
    bool m_hasOrigin = false;
};

} // namespace aerotwin
