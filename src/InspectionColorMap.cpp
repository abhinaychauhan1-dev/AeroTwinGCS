/**
 * @file     : InspectionColorMap.cpp
 * @brief    : Implements inspection coverage color mapping utilities.
 * @details  : Converts inspection values into display colors.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "InspectionColorMap.hpp"

#include <QtCore/QSize>
#include <QtCore/qfloat16.h>

#include <algorithm>
#include <cmath>

namespace {

/**
 * @brief Converts one sRGB channel to linear-light intensity.
 * @param channel sRGB channel in the range from zero to one.
 * @return Corresponding linear-light channel value.
 */
float srgbToLinear(float channel)
{
    return channel <= 0.04045f ? channel / 12.92f
                               : std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

struct LinearColor
{
    float r, g, b;
};

/**
 * @brief Converts a QColor from sRGB to a linear RGB triplet.
 * @param color Source display color.
 * @return Linear-light RGB components.
 */
LinearColor linearize(const QColor &color)
{
    const QColor rgb = color.toRgb();
    return {srgbToLinear(float(rgb.redF())),
            srgbToLinear(float(rgb.greenF())),
            srgbToLinear(float(rgb.blueF()))};
}

/**
 * @brief Linearly interpolates between two scalar values.
 * @param a Start value.
 * @param b End value.
 * @param t Interpolation factor.
 * @return Interpolated value.
 */
float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

} // namespace

/**
 * @brief Creates the floating-point inspection color-ramp texture.
 * @param parent Optional Qt object owner.
 */
InspectionColorMap::InspectionColorMap(QQuick3DObject *parent)
    : QQuick3DTextureData(parent)
{
    setFormat(QQuick3DTextureData::Format::RGBA16F);
    setHasTransparency(true);
    setSize(QSize(RampSize, 1));
    rebuild();
}

/** @brief Destroys the inspection color-ramp texture. */
InspectionColorMap::~InspectionColorMap() = default;

/**
 * @brief Sets the color used for uninspected surface regions.
 * @param color New display color.
 */
void InspectionColorMap::setUninspectedColor(const QColor &color)
{
    if (m_uninspected == color)
        return;
    m_uninspected = color;
    rebuild();
    Q_EMIT uninspectedColorChanged();
}

/**
 * @brief Sets the color used for partially inspected surface regions.
 * @param color New display color.
 */
void InspectionColorMap::setPartialColor(const QColor &color)
{
    if (m_partial == color)
        return;
    m_partial = color;
    rebuild();
    Q_EMIT partialColorChanged();
}

/**
 * @brief Sets the color used for fully inspected surface regions.
 * @param color New display color.
 */
void InspectionColorMap::setInspectedColor(const QColor &color)
{
    if (m_inspected == color)
        return;
    m_inspected = color;
    rebuild();
    Q_EMIT inspectedColorChanged();
}

/**
 * @brief Sets opacity for uninspected regions.
 * @param opacity Opacity clamped to the range from zero to one.
 */
void InspectionColorMap::setUninspectedOpacity(qreal opacity)
{
    const qreal clamped = std::clamp(opacity, qreal(0.0), qreal(1.0));
    if (qFuzzyIsNull(m_uninspectedOpacity - clamped))
        return;
    m_uninspectedOpacity = clamped;
    rebuild();
    Q_EMIT uninspectedOpacityChanged();
}

/**
 * @brief Sets opacity for partially inspected regions.
 * @param opacity Opacity clamped to the range from zero to one.
 */
void InspectionColorMap::setPartialOpacity(qreal opacity)
{
    const qreal clamped = std::clamp(opacity, qreal(0.0), qreal(1.0));
    if (qFuzzyIsNull(m_partialOpacity - clamped))
        return;
    m_partialOpacity = clamped;
    rebuild();
    Q_EMIT partialOpacityChanged();
}

/**
 * @brief Sets opacity for fully inspected regions.
 * @param opacity Opacity clamped to the range from zero to one.
 */
void InspectionColorMap::setInspectedOpacity(qreal opacity)
{
    const qreal clamped = std::clamp(opacity, qreal(0.0), qreal(1.0));
    if (qFuzzyIsNull(m_inspectedOpacity - clamped))
        return;
    m_inspectedOpacity = clamped;
    rebuild();
    Q_EMIT inspectedOpacityChanged();
}

/** @brief Rebuilds the linear-color floating-point texture ramp used by the coverage shader. */
void InspectionColorMap::rebuild()
{
    // Interpolate in linear color space so transitions have visually uniform brightness.
    const LinearColor a = linearize(m_uninspected);
    const LinearColor b = linearize(m_partial);
    const LinearColor c = linearize(m_inspected);

    const float alphaA = float(m_uninspectedOpacity);
    const float alphaB = float(m_partialOpacity);
    const float alphaC = float(m_inspectedOpacity);

    m_ramp.resize(qsizetype(RampSize) * 4 * qsizetype(sizeof(qfloat16)));
    auto *texel = reinterpret_cast<qfloat16 *>(m_ramp.data());

    for (int i = 0; i < RampSize; ++i) {
        const float t = float(i) / float(RampSize - 1);

        LinearColor colour{};
        float alpha = 0.0f;
        if (t < 0.5f) {
            const float k = t * 2.0f;
            colour = {lerp(a.r, b.r, k), lerp(a.g, b.g, k), lerp(a.b, b.b, k)};
            alpha = lerp(alphaA, alphaB, k);
        } else {
            const float k = (t - 0.5f) * 2.0f;
            colour = {lerp(b.r, c.r, k), lerp(b.g, c.g, k), lerp(b.b, c.b, k)};
            alpha = lerp(alphaB, alphaC, k);
        }

        *texel++ = qfloat16(colour.r);
        *texel++ = qfloat16(colour.g);
        *texel++ = qfloat16(colour.b);
        *texel++ = qfloat16(alpha);
    }

    setTextureData(m_ramp);
}
