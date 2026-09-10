/**
 * @file     : CoverageMapTextureData.hpp
 * @brief    : Declares texture data used for inspection coverage maps.
 * @details  : Stores coverage values for visual inspection progress.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/QQuick3DTextureData>

#include <array>
#include <atomic>
#include <span>
#include <vector>

/*!
    \class CoverageMapTextureData

    Single-channel (R8) inspection-quality map in the asset's UV space.

    Each texel holds the best quality score achieved for that patch of surface,
    where quality already folds in the drone's viewing angle and standoff
    distance (computed by the coverage solver, not here). The custom material
    samples this map and remaps it through the inspection colour ramp.

    Splats are accumulated with max() so a surface never "un-inspects" itself
    when a later pass grazes it from a worse angle.

    Threading: splat()/applyWeights() may be called from the coverage worker;
    uploads are coalesced into a single queued flush per frame and rotate
    through a small staging pool so the implicitly shared QByteArray handed to
    the scene graph is never rewritten while a consumer still holds it.
*/
class CoverageMapTextureData : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int resolution READ resolution WRITE setResolution NOTIFY resolutionChanged FINAL)
    Q_PROPERTY(qreal fullThreshold READ fullThreshold WRITE setFullThreshold NOTIFY fullThresholdChanged FINAL)
    Q_PROPERTY(qreal coveredFraction READ coveredFraction NOTIFY coveredFractionChanged FINAL)

public:
    static constexpr int MinResolution = 16;
    static constexpr int MaxResolution = 4096;
    static constexpr int StagingCount = 3;

    /** @brief Creates a default R8 coverage texture. @param parent Optional Qt object owner. */
    explicit CoverageMapTextureData(QQuick3DObject *parent = nullptr);
    /** @brief Releases coverage texture resources. */
    ~CoverageMapTextureData() override;

    /** @brief Returns texture resolution. @return Texels on each square axis. */
    [[nodiscard]] int resolution() const;
    /** @brief Sets texture resolution. @param resolution Requested texels per axis. */
    void setResolution(int resolution);

    /*! Quality at or above which a texel counts as fully inspected. */
    [[nodiscard]] qreal fullThreshold() const;
    /** @brief Sets the complete-coverage threshold. @param threshold Quality from zero to one. */
    void setFullThreshold(qreal threshold);

    /*! Ratio of texels at or above fullThreshold, published on the last flush. */
    [[nodiscard]] qreal coveredFraction() const noexcept { return m_publishedFraction; }

    /*! Paint a quality footprint centred on (u, v); \a radius is in UV units. */
    Q_INVOKABLE void splat(qreal u, qreal v, qreal radius, qreal quality);

    /** @brief Samples a UV coverage value. @param u Horizontal UV coordinate. @param v Vertical UV coordinate. @return Normalized quality. */
    Q_INVOKABLE qreal sample(qreal u, qreal v) const;

    /** @brief Clears all accumulated coverage data. */
    Q_INVOKABLE void clear();

    /*! Bulk ingest from a solver; \a weights must be resolution^2 values in 0..1. */
    void applyWeights(std::span<const float> weights);

Q_SIGNALS:
    void resolutionChanged();
    void fullThresholdChanged();
    void coveredFractionChanged();

private Q_SLOTS:
    /** @brief Uploads dirty coverage weights from the GUI thread. */
    void flushToTexture();

private:
    /** @brief Allocates cleared texture weights. @param resolution Texels per square axis. */
    void allocate(int resolution);
    /** @brief Coalesces pending GUI-thread texture uploads. */
    void scheduleFlush();

    mutable QMutex m_mutex;
    std::vector<quint8> m_weights;
    int m_resolution = 0;
    qreal m_fullThreshold = 0.85;
    bool m_dirty = false;

    std::atomic<bool> m_flushScheduled{false};

    std::array<QByteArray, StagingCount> m_staging;
    int m_stagingIndex = 0;
    qreal m_publishedFraction = 0.0;
};
