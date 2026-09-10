/**
 * @file     : FrustumGeometry.hpp
 * @brief    : Declares geometry for the drone camera frustum.
 * @details  : Provides wireframe and beam geometry for camera visualization.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include <QtCore/QByteArray>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/QQuick3DGeometry>

/*!
    \class FrustumGeometry

    Procedural camera-frustum mesh for a gimbal-mounted inspection camera.

    Uses the Qt Quick 3D camera convention: the apex sits at the local origin
    and the pyramid opens along -Z, so the geometry can be parented straight to
    the gimbal Node without any corrective rotation.

    Two styles share the same solver:
      * Wireframe - Lines primitive, apex-to-far-corner edges plus the near and
        far plane rectangles.
      * Beam      - Triangles primitive, the solid volume for a translucent
        raycast beam.

    Vertex layout (stride 28):
        offset  0 : float3 position
        offset 12 : float4 color, RGB = white, A = depth fade ramp so the beam
                    dissolves toward the far plane. Multiply it with the
                    material colour via vertexColorsEnabled.
*/
class FrustumGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal horizontalFov READ horizontalFov WRITE setHorizontalFov NOTIFY horizontalFovChanged FINAL)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio WRITE setAspectRatio NOTIFY aspectRatioChanged FINAL)
    Q_PROPERTY(qreal projectionRange READ projectionRange WRITE setProjectionRange NOTIFY projectionRangeChanged FINAL)
    Q_PROPERTY(qreal nearPlane READ nearPlane WRITE setNearPlane NOTIFY nearPlaneChanged FINAL)
    Q_PROPERTY(qreal farAlpha READ farAlpha WRITE setFarAlpha NOTIFY farAlphaChanged FINAL)
    Q_PROPERTY(Style style READ style WRITE setStyle NOTIFY styleChanged FINAL)
    Q_PROPERTY(qreal verticalFov READ verticalFov NOTIFY verticalFovChanged FINAL)

public:
    enum class Style {
        Wireframe,
        Beam,
    };
    Q_ENUM(Style)

    static constexpr qsizetype Stride = 7 * sizeof(float);
    static constexpr int PositionOffset = 0;
    static constexpr int ColorOffset = 3 * sizeof(float);

    /** @brief Creates procedural camera-frustum geometry. @param parent Optional Qt object owner. */
    explicit FrustumGeometry(QQuick3DObject *parent = nullptr);
    /** @brief Releases frustum geometry resources. */
    ~FrustumGeometry() override;

    /** @brief Returns horizontal field of view. @return Degrees. */
    [[nodiscard]] qreal horizontalFov() const noexcept { return m_horizontalFov; }
    /** @brief Sets horizontal field of view. @param degrees Field of view in degrees. */
    void setHorizontalFov(qreal degrees);

    /** @brief Returns camera aspect ratio. @return Width divided by height. */
    [[nodiscard]] qreal aspectRatio() const noexcept { return m_aspectRatio; }
    /** @brief Sets camera aspect ratio. @param aspectRatio Width divided by height. */
    void setAspectRatio(qreal aspectRatio);

    /** @brief Returns projection range. @return Distance in metres. */
    [[nodiscard]] qreal projectionRange() const noexcept { return m_projectionRange; }
    /** @brief Sets projection range. @param metres Distance in metres. */
    void setProjectionRange(qreal metres);

    /** @brief Returns near-plane distance. @return Distance in metres. */
    [[nodiscard]] qreal nearPlane() const noexcept { return m_nearPlane; }
    /** @brief Sets near-plane distance. @param metres Distance in metres. */
    void setNearPlane(qreal metres);

    /** @brief Returns far-plane opacity. @return Opacity from zero to one. */
    [[nodiscard]] qreal farAlpha() const noexcept { return m_farAlpha; }
    /** @brief Sets far-plane opacity. @param alpha Opacity from zero to one. */
    void setFarAlpha(qreal alpha);

    /** @brief Returns the active geometry style. @return Wireframe or beam style. */
    [[nodiscard]] Style style() const noexcept { return m_style; }
    /** @brief Sets the generated geometry style. @param style Wireframe or beam style. */
    void setStyle(Style style);

    /*! Derived from the horizontal FOV and the sensor aspect ratio. */
    [[nodiscard]] qreal verticalFov() const;

    /*! Far-plane corner in gimbal-local space; 0..3 counter-clockwise from top-right. */
    Q_INVOKABLE QVector3D farCorner(int index) const;

    /*! Half extents of the footprint the camera covers at \a distance metres. */
    Q_INVOKABLE QVector2D footprintAt(qreal distance) const;

Q_SIGNALS:
    void horizontalFovChanged();
    void aspectRatioChanged();
    void projectionRangeChanged();
    void nearPlaneChanged();
    void farAlphaChanged();
    void styleChanged();
    void verticalFovChanged();

private:
    /** @brief Recreates vertex and index buffers after a property update. */
    void rebuild();

    qreal m_horizontalFov = 78.0;
    qreal m_aspectRatio = 16.0 / 9.0;
    qreal m_projectionRange = 40.0;
    qreal m_nearPlane = 0.4;
    qreal m_farAlpha = 0.18;
    Style m_style = Style::Wireframe;

    QByteArray m_vertices;
    QByteArray m_indices;
};
