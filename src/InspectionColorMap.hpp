/**
 * @file     : InspectionColorMap.hpp
 * @brief    : Declares inspection coverage color mapping utilities.
 * @details  : Defines colors used to indicate inspection quality and status.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include <QtCore/QByteArray>
#include <QtGui/QColor>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/QQuick3DTextureData>

/*!
    \class InspectionColorMap

    256x1 RGBA16F lookup ramp consumed by the coverage material.

    The shader normalises raw coverage into a 0..1 "inspection state" using the
    authored thresholds, then samples this ramp:

        0.0  uninspected  (translucent red shell + hot rim)
        0.5  partial      (amber)
        1.0  inspected    (glowing green/cyan)

    Alpha carries the per-state opacity, so the asset stays see-through where
    nothing has been captured yet and turns solid as coverage completes.

    Colours are authored in sRGB (QColor) and written out linearised, because
    Qt Quick 3D shades in linear space and CustomMaterial samplers perform no
    implicit colour conversion. RGBA16F is used so the ramp keeps precision in
    the darks and still filters linearly on every RHI backend.
*/
class InspectionColorMap : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor uninspectedColor READ uninspectedColor WRITE setUninspectedColor NOTIFY uninspectedColorChanged FINAL)
    Q_PROPERTY(QColor partialColor READ partialColor WRITE setPartialColor NOTIFY partialColorChanged FINAL)
    Q_PROPERTY(QColor inspectedColor READ inspectedColor WRITE setInspectedColor NOTIFY inspectedColorChanged FINAL)
    Q_PROPERTY(qreal uninspectedOpacity READ uninspectedOpacity WRITE setUninspectedOpacity NOTIFY uninspectedOpacityChanged FINAL)
    Q_PROPERTY(qreal partialOpacity READ partialOpacity WRITE setPartialOpacity NOTIFY partialOpacityChanged FINAL)
    Q_PROPERTY(qreal inspectedOpacity READ inspectedOpacity WRITE setInspectedOpacity NOTIFY inspectedOpacityChanged FINAL)

public:
    static constexpr int RampSize = 256;

    /** @brief Creates the inspection color-ramp texture. @param parent Optional Qt object owner. */
    explicit InspectionColorMap(QQuick3DObject *parent = nullptr);
    /** @brief Releases color-ramp texture resources. */
    ~InspectionColorMap() override;

    /** @brief Returns the uninspected-region color. @return Current display color. */
    [[nodiscard]] QColor uninspectedColor() const noexcept { return m_uninspected; }
    /** @brief Sets the uninspected-region color. @param color New display color. */
    void setUninspectedColor(const QColor &color);

    /** @brief Returns the partial-coverage color. @return Current display color. */
    [[nodiscard]] QColor partialColor() const noexcept { return m_partial; }
    /** @brief Sets the partial-coverage color. @param color New display color. */
    void setPartialColor(const QColor &color);

    /** @brief Returns the inspected-region color. @return Current display color. */
    [[nodiscard]] QColor inspectedColor() const noexcept { return m_inspected; }
    /** @brief Sets the inspected-region color. @param color New display color. */
    void setInspectedColor(const QColor &color);

    /** @brief Returns uninspected-region opacity. @return Opacity from zero to one. */
    [[nodiscard]] qreal uninspectedOpacity() const noexcept { return m_uninspectedOpacity; }
    /** @brief Sets uninspected-region opacity. @param opacity Opacity from zero to one. */
    void setUninspectedOpacity(qreal opacity);

    /** @brief Returns partial-coverage opacity. @return Opacity from zero to one. */
    [[nodiscard]] qreal partialOpacity() const noexcept { return m_partialOpacity; }
    /** @brief Sets partial-coverage opacity. @param opacity Opacity from zero to one. */
    void setPartialOpacity(qreal opacity);

    /** @brief Returns inspected-region opacity. @return Opacity from zero to one. */
    [[nodiscard]] qreal inspectedOpacity() const noexcept { return m_inspectedOpacity; }
    /** @brief Sets inspected-region opacity. @param opacity Opacity from zero to one. */
    void setInspectedOpacity(qreal opacity);

Q_SIGNALS:
    void uninspectedColorChanged();
    void partialColorChanged();
    void inspectedColorChanged();
    void uninspectedOpacityChanged();
    void partialOpacityChanged();
    void inspectedOpacityChanged();

private:
    /** @brief Rebuilds the GPU texture after a color or opacity update. */
    void rebuild();

    QColor m_uninspected{QStringLiteral("#ff3b30")};
    QColor m_partial{QStringLiteral("#ffcc00")};
    QColor m_inspected{QStringLiteral("#30d158")};
    qreal m_uninspectedOpacity = 0.16;
    qreal m_partialOpacity = 0.55;
    qreal m_inspectedOpacity = 0.92;

    QByteArray m_ramp;
};
