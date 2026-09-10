/**
 * @file     : DronePointCloudGeometry.hpp
 * @brief    : Declares point-cloud geometry for the drone view.
 * @details  : Defines Qt Quick 3D geometry used to render drone point data.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtGui/QVector3D>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/QQuick3DGeometry>

#include <array>
#include <atomic>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

/*!
    Interleaved vertex as it is laid out in the GPU buffer.

        offset  0 : float3 position   (x, y, z)   - PositionSemantic
        offset 12 : float4 color      (r, g, b, a) - ColorSemantic  (a doubles
                                                     as LiDAR intensity)
        stride 28

    Kept trivially copyable so packets can be blitted with a single memcpy.
*/
struct DronePoint
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

static_assert(std::is_trivially_copyable_v<DronePoint>);
static_assert(sizeof(DronePoint) == 7 * sizeof(float),
              "DronePoint must stay tightly packed - it is uploaded verbatim.");

/*!
    \class DronePointCloudGeometry

    Streaming point-cloud geometry for a drone SLAM/LiDAR feed.

    Threading contract
    ------------------
    * appendPoints() / replacePoints() / clear() are thread-safe and are meant
      to be called from the sensor worker thread. They only take a short mutex
      and memcpy into a pre-allocated ring; they never allocate.
    * A single coalesced queued invocation is posted to the object's own
      (GUI) thread. No matter how many packets a worker pushes between two
      frames, at most one flush is scheduled, so the event loop cannot be
      flooded at 60 Hz.
    * flushToGeometry() runs on the GUI thread, copies the ring into the next
      GPU staging buffer, updates bounds and calls markAllDirty()/update().

    Allocation contract
    -------------------
    Every buffer (ring + GPU staging pool) is sized once in setCapacity().
    The steady-state per-frame cost is two linear passes over the live points
    (one memcpy, one min/max) and zero heap traffic.

    Because QQuick3DGeometry stores the vertex QByteArray implicitly shared -
    and the render thread may still hold the previous frame's copy - the
    staging buffers are rotated through a pool of GpuBufferCount entries so a
    buffer is only rewritten once every consumer has dropped its reference.
*/
class DronePointCloudGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int capacity READ capacity WRITE setCapacity NOTIFY capacityChanged FINAL)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY pointCountChanged FINAL)
    Q_PROPERTY(bool ringMode READ ringMode WRITE setRingMode NOTIFY ringModeChanged FINAL)
    Q_PROPERTY(qulonglong droppedPoints READ droppedPoints NOTIFY droppedPointsChanged FINAL)

public:
    static constexpr int MaxCapacity = 250'000;
    static constexpr int DefaultCapacity = 250'000;
    static constexpr qsizetype Stride = static_cast<qsizetype>(sizeof(DronePoint));
    static constexpr int PositionOffset = 0;
    static constexpr int ColorOffset = 3 * sizeof(float);
    static constexpr int GpuBufferCount = 3;

    /** @brief Creates streaming point-cloud geometry. @param parent Optional Qt object owner. */
    explicit DronePointCloudGeometry(QQuick3DObject *parent = nullptr);
    /** @brief Releases point-cloud geometry resources. */
    ~DronePointCloudGeometry() override;

    /** @brief Returns the retained-point capacity. @return Maximum point count. */
    [[nodiscard]] int capacity() const;
    /** @brief Sets the retained-point capacity. @param capacity Requested maximum point count. */
    void setCapacity(int capacity);

    /*! Points published to the scene graph on the last flush. */
    [[nodiscard]] int pointCount() const noexcept { return m_publishedCount; }

    /*! true: oldest points are overwritten. false: the ring fills and stops. */
    /** @brief Returns whether full buffers overwrite the oldest points. @return True when wrapping is enabled. */
    [[nodiscard]] bool ringMode() const;
    /** @brief Sets full-buffer behavior. @param ringMode True to overwrite oldest points. */
    void setRingMode(bool ringMode);

    [[nodiscard]] qulonglong droppedPoints() const noexcept
    {
        return m_droppedPoints.load(std::memory_order_relaxed);
    }

    // --- Producer API (any thread) ---------------------------------------

    /*! Push a sensor packet. Zero-copy ingestion, single memcpy into the ring. */
    void appendPoints(std::span<const DronePoint> points);

    /*! Drop the current cloud and install \a points as the whole frame. */
    void replacePoints(std::span<const DronePoint> points);

    /** @brief Removes all points from the retained cloud. */
    Q_INVOKABLE void clear();

Q_SIGNALS:
    void capacityChanged();
    void pointCountChanged();
    void ringModeChanged();
    void droppedPointsChanged();

private Q_SLOTS:
    /** @brief Uploads the latest stable point snapshot on the GUI thread. */
    void flushToGeometry();

private:
    /** @brief Declares the fixed GPU vertex layout. */
    void configureAttributes();
    /** @brief Coalesces a pending GUI-thread geometry upload. */
    void scheduleFlush();
    /** @brief Records points rejected by capacity limits. @param count Number of dropped points. */
    void recordDropped(std::size_t count);

    // --- Shared state (guarded by m_mutex) -------------------------------
    mutable QMutex m_mutex;
    std::vector<DronePoint> m_ring;
    int m_capacity = 0;
    int m_head = 0;          // next write slot
    int m_size = 0;          // live points, always contiguous from index 0
    bool m_ringMode = true;
    bool m_cloudDirty = false;

    std::atomic<qulonglong> m_droppedPoints{0};
    std::atomic<bool> m_flushScheduled{false};

    // --- GUI-thread only --------------------------------------------------
    std::array<QByteArray, GpuBufferCount> m_gpuBuffers;
    int m_gpuIndex = 0;
    int m_publishedCount = 0;
    qulonglong m_lastReportedDrops = 0;
};
