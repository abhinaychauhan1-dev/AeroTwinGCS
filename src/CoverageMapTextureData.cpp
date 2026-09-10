/**
 * @file     : CoverageMapTextureData.cpp
 * @brief    : Implements texture data used for coverage maps.
 * @details  : Updates inspection coverage texture values for rendering.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "CoverageMapTextureData.hpp"

#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QSize>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr int kDefaultResolution = 512;

/**
 * @brief Converts normalized coverage quality to an R8 texel value.
 * @param value Quality value expected in the range from zero to one.
 * @return Clamped 8-bit texture value.
 */
quint8 toByte(qreal value)
{
    return quint8(std::lround(std::clamp(value, qreal(0.0), qreal(1.0)) * 255.0));
}

} // namespace

/**
 * @brief Creates an R8 coverage texture at the default resolution.
 * @param parent Optional Qt object owner.
 */
CoverageMapTextureData::CoverageMapTextureData(QQuick3DObject *parent)
    : QQuick3DTextureData(parent)
{
    setFormat(QQuick3DTextureData::Format::R8);
    setHasTransparency(false);
    setResolution(kDefaultResolution);
}

/** @brief Destroys the coverage texture after Qt releases its resources. */
CoverageMapTextureData::~CoverageMapTextureData() = default;

/**
 * @brief Returns the square coverage-map resolution.
 * @return Number of texels on each texture axis.
 */
int CoverageMapTextureData::resolution() const
{
    QMutexLocker locker(&m_mutex);
    return m_resolution;
}

/**
 * @brief Resizes and clears the coverage texture.
 * @param resolution Requested square texture resolution.
 */
void CoverageMapTextureData::setResolution(int resolution)
{
    const int clamped = std::clamp(resolution, MinResolution, MaxResolution);
    {
        QMutexLocker locker(&m_mutex);
        if (clamped == m_resolution)
            return;
        allocate(clamped);
    }

    const qsizetype bytes = qsizetype(clamped) * qsizetype(clamped);
    for (QByteArray &buffer : m_staging) {
        buffer.clear();
        buffer.reserve(bytes);
    }
    m_stagingIndex = 0;

    setSize(QSize(clamped, clamped));
    Q_EMIT resolutionChanged();
    scheduleFlush();
}

/**
 * @brief Allocates cleared coverage weights under the caller-held mutex.
 * @param resolution Square texture resolution to allocate.
 */
void CoverageMapTextureData::allocate(int resolution)
{
    m_weights.assign(std::size_t(resolution) * std::size_t(resolution), 0);
    m_resolution = resolution;
    m_dirty = true;
}

/**
 * @brief Returns the threshold that marks a texel as fully covered.
 * @return Coverage threshold in the range from zero to one.
 */
qreal CoverageMapTextureData::fullThreshold() const
{
    QMutexLocker locker(&m_mutex);
    return m_fullThreshold;
}

/**
 * @brief Sets the minimum quality that counts as complete coverage.
 * @param threshold Coverage threshold, clamped to the range from zero to one.
 */
void CoverageMapTextureData::setFullThreshold(qreal threshold)
{
    const qreal clamped = std::clamp(threshold, qreal(0.0), qreal(1.0));
    {
        QMutexLocker locker(&m_mutex);
        if (qFuzzyIsNull(m_fullThreshold - clamped))
            return;
        m_fullThreshold = clamped;
        m_dirty = true;
    }
    Q_EMIT fullThresholdChanged();
    scheduleFlush();
}

/**
 * @brief Paints a quality-weighted circular footprint in UV space.
 * @param u Horizontal UV coordinate of the footprint centre.
 * @param v Vertical UV coordinate of the footprint centre.
 * @param radius Footprint radius in UV units.
 * @param quality Maximum quality contributed by the footprint.
 */
void CoverageMapTextureData::splat(qreal u, qreal v, qreal radius, qreal quality)
{
    const quint8 peak = toByte(quality);
    if (peak == 0)
        return;

    {
        QMutexLocker locker(&m_mutex);
        if (m_resolution == 0)
            return;

        const qreal r = std::max(radius, qreal(0.0));
        const qreal texelRadius = r * m_resolution;
        const qreal cx = u * m_resolution;
        const qreal cy = v * m_resolution;

        const int x0 = std::max(0, int(std::floor(cx - texelRadius)));
        const int x1 = std::min(m_resolution - 1, int(std::ceil(cx + texelRadius)));
        const int y0 = std::max(0, int(std::floor(cy - texelRadius)));
        const int y1 = std::min(m_resolution - 1, int(std::ceil(cy + texelRadius)));
        if (x0 > x1 || y0 > y1)
            return;

        const qreal invRadius = texelRadius > 0.0 ? 1.0 / texelRadius : 0.0;

        for (int y = y0; y <= y1; ++y) {
            quint8 *row = m_weights.data() + std::size_t(y) * std::size_t(m_resolution);
            const qreal dy = (y + 0.5) - cy;
            for (int x = x0; x <= x1; ++x) {
                const qreal dx = (x + 0.5) - cx;
                const qreal d = std::sqrt(dx * dx + dy * dy) * invRadius;
                if (d > 1.0)
                    continue;
                // Smoothstep falloff keeps splat borders from banding.
                const qreal t = 1.0 - d;
                const qreal falloff = t * t * (3.0 - 2.0 * t);
                row[x] = std::max(row[x], toByte(quality * falloff));
            }
        }
        m_dirty = true;
    }

    scheduleFlush();
}

/**
 * @brief Samples the nearest stored coverage value at a UV coordinate.
 * @param u Horizontal UV coordinate.
 * @param v Vertical UV coordinate.
 * @return Normalized coverage quality, clamped to valid texture bounds.
 */
qreal CoverageMapTextureData::sample(qreal u, qreal v) const
{
    QMutexLocker locker(&m_mutex);
    if (m_resolution == 0)
        return 0.0;

    const int x = std::clamp(int(u * m_resolution), 0, m_resolution - 1);
    const int y = std::clamp(int(v * m_resolution), 0, m_resolution - 1);
    return m_weights[std::size_t(y) * std::size_t(m_resolution) + std::size_t(x)] / 255.0;
}

/** @brief Clears all stored coverage values and schedules a texture upload. */
void CoverageMapTextureData::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        std::fill(m_weights.begin(), m_weights.end(), quint8(0));
        m_dirty = true;
    }
    scheduleFlush();
}

/**
 * @brief Replaces the coverage map with normalized solver output.
 * @param weights One quality value for every texel, in row-major order.
 */
void CoverageMapTextureData::applyWeights(std::span<const float> weights)
{
    {
        QMutexLocker locker(&m_mutex);
        if (weights.size() != m_weights.size())
            return;
        std::transform(weights.begin(), weights.end(), m_weights.begin(), [](float w) {
            return toByte(qreal(w));
        });
        m_dirty = true;
    }
    scheduleFlush();
}

/** @brief Queues at most one GUI-thread texture upload for pending coverage changes. */
void CoverageMapTextureData::scheduleFlush()
{
    if (m_flushScheduled.exchange(true, std::memory_order_acq_rel))
        return;
    QMetaObject::invokeMethod(this, "flushToTexture", Qt::QueuedConnection);
}

/** @brief Copies dirty worker data into a rotating GUI-thread texture staging buffer. */
void CoverageMapTextureData::flushToTexture()
{
    // Release the gate before copying so updates arriving during the upload queue another flush.
    m_flushScheduled.store(false, std::memory_order_release);

    QByteArray &staging = m_staging[m_stagingIndex];
    qreal fraction = 0.0;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_dirty || m_weights.empty())
            return;
        m_dirty = false;

        staging.resize(qsizetype(m_weights.size()));
        std::memcpy(staging.data(), m_weights.data(), m_weights.size());

        const quint8 cut = toByte(m_fullThreshold);
        const auto covered = std::count_if(m_weights.cbegin(), m_weights.cend(), [cut](quint8 w) {
            return w >= cut;
        });
        fraction = qreal(covered) / qreal(m_weights.size());
    }

    m_stagingIndex = (m_stagingIndex + 1) % StagingCount;

    setTextureData(staging);

    if (!qFuzzyIsNull(fraction - m_publishedFraction)) {
        m_publishedFraction = fraction;
        Q_EMIT coveredFractionChanged();
    }
}
