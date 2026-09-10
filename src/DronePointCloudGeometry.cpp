/**
 * @file     : DronePointCloudGeometry.cpp
 * @brief    : Implements point-cloud geometry for the drone view.
 * @details  : Builds renderable vertex data for Qt Quick 3D.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "DronePointCloudGeometry.hpp"

#include <QtCore/QMutexLocker>
#include <QtCore/QMetaObject>

#include <algorithm>
#include <cstring>
#include <limits>

namespace {

constexpr float kFloatMax = std::numeric_limits<float>::max();
constexpr float kFloatLowest = std::numeric_limits<float>::lowest();

} // namespace

/**
 * @brief Creates an empty point-cloud geometry with its default capacity.
 * @param parent Optional Qt object owner.
 */
DronePointCloudGeometry::DronePointCloudGeometry(QQuick3DObject *parent)
    : QQuick3DGeometry(parent)
{
    configureAttributes();
    setCapacity(DefaultCapacity);
}

/** @brief Destroys the point-cloud geometry after Qt releases its resources. */
DronePointCloudGeometry::~DronePointCloudGeometry() = default;

/** @brief Configures the fixed vertex layout shared by every point-cloud upload. */
void DronePointCloudGeometry::configureAttributes()
{
    // Declared exactly once; the per-frame path only swaps the vertex bytes.
    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Points);
    setStride(static_cast<int>(Stride));
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,
                 PositionOffset,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::ColorSemantic,
                 ColorOffset,
                 QQuick3DGeometry::Attribute::F32Type);
    setBounds(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 0.0f));
}

/**
 * @brief Returns the maximum number of points retained in the ring buffer.
 * @return Configured point capacity.
 */
int DronePointCloudGeometry::capacity() const
{
    QMutexLocker locker(&m_mutex);
    return m_capacity;
}

/**
 * @brief Resizes and clears the CPU point ring and GPU staging buffers.
 * @param capacity Requested maximum retained point count.
 */
void DronePointCloudGeometry::setCapacity(int capacity)
{
    const int clamped = std::clamp(capacity, 0, MaxCapacity);

    {
        QMutexLocker locker(&m_mutex);
        if (clamped == m_capacity)
            return;

        // The only allocation site in the whole class.
        m_ring.assign(static_cast<std::size_t>(clamped), DronePoint{});
        m_ring.shrink_to_fit();
        m_capacity = clamped;
        m_head = 0;
        m_size = 0;
        m_cloudDirty = true;
    }

    const qsizetype byteCapacity = static_cast<qsizetype>(clamped) * Stride;
    for (QByteArray &buffer : m_gpuBuffers) {
        buffer.clear();
        buffer.reserve(byteCapacity); // resize() inside the loop stays realloc-free
    }
    m_gpuIndex = 0;

    Q_EMIT capacityChanged();
    scheduleFlush();
}

/**
 * @brief Reports whether new points overwrite the oldest retained points.
 * @return True when the ring buffer is allowed to wrap.
 */
bool DronePointCloudGeometry::ringMode() const
{
    QMutexLocker locker(&m_mutex);
    return m_ringMode;
}

/**
 * @brief Enables or disables overwrite behavior when the point buffer is full.
 * @param ringMode True to retain the newest points by overwriting old entries.
 */
void DronePointCloudGeometry::setRingMode(bool ringMode)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_ringMode == ringMode)
            return;
        m_ringMode = ringMode;
    }
    Q_EMIT ringModeChanged();
}

/**
 * @brief Adds a dropped-point count without synchronizing the producer path.
 * @param count Number of points discarded due to capacity limits.
 */
void DronePointCloudGeometry::recordDropped(std::size_t count)
{
    if (count > 0)
        m_droppedPoints.fetch_add(static_cast<qulonglong>(count), std::memory_order_relaxed);
}

    /**
     * @brief Appends points to the retained cloud.
     * @param points Point span supplied by the producer thread.
     */
void DronePointCloudGeometry::appendPoints(std::span<const DronePoint> points)
{
    if (points.empty())
        return;

    {
        QMutexLocker locker(&m_mutex);
        if (m_capacity == 0) {
            recordDropped(points.size());
            return;
        }

        std::span<const DronePoint> src = points;

        // A packet larger than the ring: keep the newest tail, count the rest.
        if (src.size() > static_cast<std::size_t>(m_capacity)) {
            recordDropped(src.size() - static_cast<std::size_t>(m_capacity));
            src = src.last(static_cast<std::size_t>(m_capacity));
        }

        if (!m_ringMode) {
            const std::size_t freeSlots = static_cast<std::size_t>(m_capacity - m_size);
            if (src.size() > freeSlots) {
                recordDropped(src.size() - freeSlots);
                src = src.first(freeSlots);
            }
            if (src.empty())
                return;
        }

        // At most two memcpys: [head, capacity) then the wrapped remainder.
        std::size_t written = 0;
        while (written < src.size()) {
            const std::size_t chunk =
                std::min(src.size() - written, static_cast<std::size_t>(m_capacity - m_head));
            std::memcpy(m_ring.data() + m_head, src.data() + written, chunk * sizeof(DronePoint));
            written += chunk;
            m_head = static_cast<int>((m_head + chunk) % static_cast<std::size_t>(m_capacity));
        }

        m_size = std::min(m_capacity, m_size + static_cast<int>(src.size()));
        m_cloudDirty = true;
    }

    scheduleFlush();
}

/**
 * @brief Replaces the retained cloud with the supplied points.
 * @param points New point set, retaining the newest values when oversized.
 */
void DronePointCloudGeometry::replacePoints(std::span<const DronePoint> points)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_capacity == 0) {
            recordDropped(points.size());
            return;
        }

        std::span<const DronePoint> src = points;
        if (src.size() > static_cast<std::size_t>(m_capacity)) {
            recordDropped(src.size() - static_cast<std::size_t>(m_capacity));
            src = src.last(static_cast<std::size_t>(m_capacity));
        }

        if (!src.empty())
            std::memcpy(m_ring.data(), src.data(), src.size() * sizeof(DronePoint));

        m_size = static_cast<int>(src.size());
        m_head = m_size % m_capacity;
        m_cloudDirty = true;
    }

    scheduleFlush();
}

/** @brief Removes all retained points and schedules an empty geometry upload. */
void DronePointCloudGeometry::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_head = 0;
        m_size = 0;
        m_cloudDirty = true;
    }
    scheduleFlush();
}

/** @brief Coalesces producer updates into a single queued GUI-thread upload. */
void DronePointCloudGeometry::scheduleFlush()
{
    // Coalescing gate: many packets per frame, at most one queued metacall.
    if (m_flushScheduled.exchange(true, std::memory_order_acq_rel))
        return;

    QMetaObject::invokeMethod(this, "flushToGeometry", Qt::QueuedConnection);
}

/** @brief Uploads a stable point-cloud snapshot and recomputes its scene bounds. */
void DronePointCloudGeometry::flushToGeometry()
{
    // Reopen the gate first so packets arriving during the copy are not lost.
    m_flushScheduled.store(false, std::memory_order_release);

    // Rotate staging buffers so the scene graph never observes a rewritten array.
    QByteArray &staging = m_gpuBuffers[m_gpuIndex];
    int count = 0;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_cloudDirty)
            return;
        m_cloudDirty = false;
        count = m_size;

        // The live region is always contiguous: [0, size) while filling, and
        // the whole ring once wrapped. Point primitives are order-independent,
        // so a wrapped ring needs no linearisation - just one memcpy.
        staging.resize(static_cast<qsizetype>(count) * Stride);
        if (count > 0)
            std::memcpy(staging.data(), m_ring.data(), static_cast<std::size_t>(count) * sizeof(DronePoint));
    }

    m_gpuIndex = (m_gpuIndex + 1) % GpuBufferCount;

    QVector3D minBounds(0.0f, 0.0f, 0.0f);
    QVector3D maxBounds(0.0f, 0.0f, 0.0f);

    if (count > 0) {
        float minX = kFloatMax, minY = kFloatMax, minZ = kFloatMax;
        float maxX = kFloatLowest, maxY = kFloatLowest, maxZ = kFloatLowest;

        const auto *points = reinterpret_cast<const DronePoint *>(staging.constData());
        for (int i = 0; i < count; ++i) {
            const DronePoint &p = points[i];
            minX = std::min(minX, p.x);
            minY = std::min(minY, p.y);
            minZ = std::min(minZ, p.z);
            maxX = std::max(maxX, p.x);
            maxY = std::max(maxY, p.y);
            maxZ = std::max(maxZ, p.z);
        }

        minBounds = QVector3D(minX, minY, minZ);
        maxBounds = QVector3D(maxX, maxY, maxZ);
    }

    setVertexData(staging);
    setBounds(minBounds, maxBounds);

    markAllDirty();
    update();

    if (m_publishedCount != count) {
        m_publishedCount = count;
        Q_EMIT pointCountChanged();
    }

    if (m_lastReportedDrops != droppedPoints()) {
        m_lastReportedDrops = droppedPoints();
        Q_EMIT droppedPointsChanged();
    }
}
