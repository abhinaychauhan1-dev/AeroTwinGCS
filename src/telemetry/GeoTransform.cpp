/**
 * @file     : GeoTransform.cpp
 * @brief    : Implements geographic coordinate transformations.
 * @details  : Converts global drone positions into local scene coordinates.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "GeoTransform.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace aerotwin {

namespace {

constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kRadToDeg = 180.0 / std::numbers::pi;

/*! NED -> scene change of basis: (n, e, d) -> (e, -d, -n). det(M) = +1. */
constexpr std::array<double, 9> kNedToScene{
    0.0, 1.0, 0.0,
    0.0, 0.0, -1.0,
    -1.0, 0.0, 0.0,
};

} // namespace

/**
 * @brief Sets the geodetic reference used by local coordinate conversion.
 * @param origin Latitude, longitude, and altitude of the ENU origin.
 */
void GeoTransform::setOrigin(const GeoPoint &origin) noexcept
{
    m_origin = origin;
    m_originEcef = geodeticToEcef(origin);

    const double lat = origin.latitudeDeg * kDegToRad;
    const double lon = origin.longitudeDeg * kDegToRad;
    m_sinLat = std::sin(lat);
    m_cosLat = std::cos(lat);
    m_sinLon = std::sin(lon);
    m_cosLon = std::cos(lon);
    m_hasOrigin = true;
}

/**
 * @brief Converts WGS-84 geodetic coordinates to Earth-centred coordinates.
 * @param point Geographic point to convert.
 * @return ECEF position in metres.
 */
Vec3 GeoTransform::geodeticToEcef(const GeoPoint &point) noexcept
{
    const double lat = point.latitudeDeg * kDegToRad;
    const double lon = point.longitudeDeg * kDegToRad;
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);

    // Radius of curvature in the prime vertical.
    const double n = SemiMajorAxis / std::sqrt(1.0 - EccentricitySquared * sinLat * sinLat);
    const double h = point.altitudeAmsl;

    return {(n + h) * cosLat * cosLon,
            (n + h) * cosLat * sinLon,
            (n * (1.0 - EccentricitySquared) + h) * sinLat};
}

/**
 * @brief Converts a geographic point into local east-north-up metres.
 * @param point Geographic point to convert.
 * @return ENU position, or a zero vector when no origin is configured.
 */
Vec3 GeoTransform::geodeticToEnu(const GeoPoint &point) const noexcept
{
    if (!m_hasOrigin)
        return {};

    const Vec3 ecef = geodeticToEcef(point);
    const double dx = ecef.x - m_originEcef.x;
    const double dy = ecef.y - m_originEcef.y;
    const double dz = ecef.z - m_originEcef.z;

    return {
        -m_sinLon * dx + m_cosLon * dy,
        -m_sinLat * m_cosLon * dx - m_sinLat * m_sinLon * dy + m_cosLat * dz,
        m_cosLat * m_cosLon * dx + m_cosLat * m_sinLon * dy + m_sinLat * dz,
    };
}

/**
 * @brief Normalizes a quaternion while handling an invalid zero-length input.
 * @param q Quaternion to normalize.
 * @return Unit quaternion, or identity for a degenerate input.
 */
Quat GeoTransform::normalize(const Quat &q) noexcept
{
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (n < 1e-12)
        return {1.0, 0.0, 0.0, 0.0};
    const double inv = 1.0 / n;
    return {q.w * inv, q.x * inv, q.y * inv, q.z * inv};
}

/**
 * @brief Converts an orientation quaternion to roll, pitch, and yaw degrees.
 * @param raw Aircraft orientation quaternion.
 * @return Euler angles in degrees.
 */
EulerAngles GeoTransform::quaternionToEuler(const Quat &raw) noexcept
{
    const Quat q = normalize(raw);

    const double roll = std::atan2(2.0 * (q.w * q.x + q.y * q.z),
                                   1.0 - 2.0 * (q.x * q.x + q.y * q.y));

    const double sinPitch = std::clamp(2.0 * (q.w * q.y - q.z * q.x), -1.0, 1.0);
    const double pitch = std::asin(sinPitch);

    const double yaw = std::atan2(2.0 * (q.w * q.z + q.x * q.y),
                                  1.0 - 2.0 * (q.y * q.y + q.z * q.z));

    return {roll * kRadToDeg, pitch * kRadToDeg, yaw * kRadToDeg};
}

/**
 * @brief Converts an aircraft body rotation into the Qt scene coordinate frame.
 * @param raw Aircraft orientation quaternion in the NED convention.
 * @return Row-major 3x3 scene rotation matrix.
 */
std::array<float, 9> GeoTransform::bodyToSceneMatrix(const Quat &raw) noexcept
{
    const Quat q = normalize(raw);

    // Body -> NED rotation matrix from the Hamilton quaternion.
    const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    const std::array<double, 9> r{
        1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz),       2.0 * (xz + wy),
        2.0 * (xy + wz),       1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx),
        2.0 * (xz - wy),       2.0 * (yz + wx),       1.0 - 2.0 * (xx + yy),
    };

    // R_scene = M * R * M^T. The body axes are remapped by the same M, because
    // FRD -> right/up/back is exactly the NED -> scene permutation.
    std::array<double, 9> mr{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 3; ++k)
                sum += kNedToScene[i * 3 + k] * r[k * 3 + j];
            mr[i * 3 + j] = sum;
        }
    }

    std::array<float, 9> out{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 3; ++k)
                sum += mr[i * 3 + k] * kNedToScene[j * 3 + k]; // M^T
            out[i * 3 + j] = float(sum);
        }
    }
    return out;
}

} // namespace aerotwin
