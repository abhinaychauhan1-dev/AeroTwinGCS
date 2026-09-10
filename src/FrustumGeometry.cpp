/**
 * @file     : FrustumGeometry.cpp
 * @brief    : Implements the drone camera frustum geometry.
 * @details  : Generates the geometric data for camera beam rendering.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "FrustumGeometry.hpp"

#include <QtCore/QList>
#include <QtCore/QtMath>
#include <QtGui/QVector2D>

#include <algorithm>
#include <array>
#include <utility>

namespace {

constexpr qreal kMinFov = 1.0;
constexpr qreal kMaxFov = 179.0;

/**
 * @brief Tests whether two real values are approximately equal.
 * @param a First value.
 * @param b Second value.
 * @return True when Qt considers the difference negligible.
 */
bool nearlyEqual(qreal a, qreal b)
{
    return qFuzzyIsNull(a - b);
}

/**
 * @brief Writes one interleaved position-and-color vertex.
 * @param cursor Output cursor advanced past the written vertex.
 * @param position Vertex position in camera-local space.
 * @param alpha Vertex alpha for depth fading.
 */
void writeVertex(float *&cursor, const QVector3D &position, float alpha)
{
    *cursor++ = position.x();
    *cursor++ = position.y();
    *cursor++ = position.z();
    *cursor++ = 1.0f;
    *cursor++ = 1.0f;
    *cursor++ = 1.0f;
    *cursor++ = alpha;
}

} // namespace

/**
 * @brief Creates frustum geometry and builds its initial buffers.
 * @param parent Optional Qt object owner.
 */
FrustumGeometry::FrustumGeometry(QQuick3DObject *parent)
    : QQuick3DGeometry(parent)
{
    rebuild();
}

/** @brief Destroys the frustum geometry after Qt releases its resources. */
FrustumGeometry::~FrustumGeometry() = default;

/**
 * @brief Sets the horizontal camera field of view.
 * @param degrees Field of view in degrees, clamped away from degenerate extremes.
 */
void FrustumGeometry::setHorizontalFov(qreal degrees)
{
    const qreal clamped = std::clamp(degrees, kMinFov, kMaxFov);
    if (nearlyEqual(m_horizontalFov, clamped))
        return;
    m_horizontalFov = clamped;
    rebuild();
    Q_EMIT horizontalFovChanged();
    Q_EMIT verticalFovChanged();
}

/**
 * @brief Sets the camera image aspect ratio.
 * @param aspectRatio Width divided by height, clamped to a positive minimum.
 */
void FrustumGeometry::setAspectRatio(qreal aspectRatio)
{
    const qreal clamped = std::max(qreal(0.01), aspectRatio);
    if (nearlyEqual(m_aspectRatio, clamped))
        return;
    m_aspectRatio = clamped;
    rebuild();
    Q_EMIT aspectRatioChanged();
    Q_EMIT verticalFovChanged();
}

/**
 * @brief Sets the far extent of the visualized camera frustum.
 * @param metres Projection range in metres.
 */
void FrustumGeometry::setProjectionRange(qreal metres)
{
    const qreal clamped = std::max(qreal(0.001), metres);
    if (nearlyEqual(m_projectionRange, clamped))
        return;
    m_projectionRange = clamped;
    rebuild();
    Q_EMIT projectionRangeChanged();
}

/**
 * @brief Sets the optional near-plane distance for wireframe rendering.
 * @param metres Near-plane distance in metres.
 */
void FrustumGeometry::setNearPlane(qreal metres)
{
    const qreal clamped = std::max(qreal(0.0), metres);
    if (nearlyEqual(m_nearPlane, clamped))
        return;
    m_nearPlane = clamped;
    rebuild();
    Q_EMIT nearPlaneChanged();
}

/**
 * @brief Sets the alpha value at the far end of the frustum.
 * @param alpha Opacity clamped to the range from zero to one.
 */
void FrustumGeometry::setFarAlpha(qreal alpha)
{
    const qreal clamped = std::clamp(alpha, qreal(0.0), qreal(1.0));
    if (nearlyEqual(m_farAlpha, clamped))
        return;
    m_farAlpha = clamped;
    rebuild();
    Q_EMIT farAlphaChanged();
}

/**
 * @brief Selects solid-beam or wireframe frustum geometry.
 * @param style Geometry style to generate.
 */
void FrustumGeometry::setStyle(Style style)
{
    if (m_style == style)
        return;
    m_style = style;
    rebuild();
    Q_EMIT styleChanged();
}

/**
 * @brief Calculates the vertical field of view from horizontal FOV and aspect ratio.
 * @return Vertical field of view in degrees.
 */
qreal FrustumGeometry::verticalFov() const
{
    const qreal tanHalfH = std::tan(qDegreesToRadians(m_horizontalFov) * 0.5);
    return qRadiansToDegrees(2.0 * std::atan(tanHalfH / m_aspectRatio));
}

/**
 * @brief Calculates half-width and half-height of the projected footprint.
 * @param distance Distance from the camera apex in metres.
 * @return Footprint half-extents in scene units.
 */
QVector2D FrustumGeometry::footprintAt(qreal distance) const
{
    const qreal tanHalfH = std::tan(qDegreesToRadians(m_horizontalFov) * 0.5);
    const qreal halfWidth = tanHalfH * distance;
    return QVector2D(float(halfWidth), float(halfWidth / m_aspectRatio));
}

/**
 * @brief Returns one far-plane corner in camera-local coordinates.
 * @param index Corner index, clamped to the four valid corners.
 * @return Far-plane corner position.
 */
QVector3D FrustumGeometry::farCorner(int index) const
{
    const QVector2D half = footprintAt(m_projectionRange);
    static constexpr std::array<std::pair<float, float>, 4> kSigns{
        std::pair{1.0f, 1.0f}, std::pair{-1.0f, 1.0f}, std::pair{-1.0f, -1.0f}, std::pair{1.0f, -1.0f}};

    const auto [sx, sy] = kSigns[std::clamp(index, 0, 3)];
    return QVector3D(sx * half.x(), sy * half.y(), -float(m_projectionRange));
}

/** @brief Rebuilds vertex and index buffers after a frustum property changes. */
void FrustumGeometry::rebuild()
{
    // clear() drops the attribute declarations too, so they are re-added below.
    clear();

    const bool beam = (m_style == Style::Beam);
    const bool hasNearRect = !beam && m_nearPlane > 0.0 && m_nearPlane < m_projectionRange;

    const QVector2D farHalf = footprintAt(m_projectionRange);
    const QVector2D nearHalf = footprintAt(m_nearPlane);

    static constexpr std::array<std::pair<float, float>, 4> kSigns{
        std::pair{1.0f, 1.0f}, std::pair{-1.0f, 1.0f}, std::pair{-1.0f, -1.0f}, std::pair{1.0f, -1.0f}};

    constexpr int kVertexCount = 9; // apex + 4 near + 4 far
    m_vertices.resize(kVertexCount * Stride);
    float *cursor = reinterpret_cast<float *>(m_vertices.data());

    const float farAlpha = float(m_farAlpha);
    const float nearRatio = m_projectionRange > 0.0
        ? float(std::clamp(m_nearPlane / m_projectionRange, qreal(0.0), qreal(1.0)))
        : 0.0f;
    const float nearAlpha = 1.0f + (farAlpha - 1.0f) * nearRatio;

    writeVertex(cursor, QVector3D(0.0f, 0.0f, 0.0f), 1.0f);

    for (const auto &[sx, sy] : kSigns) {
        writeVertex(cursor,
                    QVector3D(sx * nearHalf.x(), sy * nearHalf.y(), -float(m_nearPlane)),
                    nearAlpha);
    }
    for (const auto &[sx, sy] : kSigns) {
        writeVertex(cursor,
                    QVector3D(sx * farHalf.x(), sy * farHalf.y(), -float(m_projectionRange)),
                    farAlpha);
    }

    // Far corners occupy indices 5..8, near corners 1..4; index order defines faces or lines.
    QList<quint16> indices;
    if (beam) {
        indices = {
            // Four side faces from the apex to the far quad.
            0, 5, 6,
            0, 6, 7,
            0, 7, 8,
            0, 8, 5,
            // Far cap, so the beam reads as a closed volume from any angle.
            5, 6, 7,
            5, 7, 8,
        };
    } else {
        indices = {
            0, 5, 0, 6, 0, 7, 0, 8, // apex to far corners
            5, 6, 6, 7, 7, 8, 8, 5, // far plane rectangle
        };
        if (hasNearRect)
            indices += QList<quint16>{1, 2, 2, 3, 3, 4, 4, 1};
    }

    m_indices.resize(qsizetype(indices.size()) * qsizetype(sizeof(quint16)));
    std::copy(indices.cbegin(), indices.cend(), reinterpret_cast<quint16 *>(m_indices.data()));

    setPrimitiveType(beam ? QQuick3DGeometry::PrimitiveType::Triangles
                          : QQuick3DGeometry::PrimitiveType::Lines);
    setStride(int(Stride));
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,
                 PositionOffset,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::ColorSemantic,
                 ColorOffset,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::IndexSemantic,
                 0,
                 QQuick3DGeometry::Attribute::U16Type);

    setVertexData(m_vertices);
    setIndexData(m_indices);

    const float maxHalfX = std::max(farHalf.x(), nearHalf.x());
    const float maxHalfY = std::max(farHalf.y(), nearHalf.y());
    setBounds(QVector3D(-maxHalfX, -maxHalfY, -float(m_projectionRange)),
              QVector3D(maxHalfX, maxHalfY, 0.0f));

    markAllDirty();
    update();
}
