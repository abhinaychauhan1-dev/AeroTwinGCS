/**
 * @file     : TelemetryIngestCore.hpp
 * @brief    : Declares the telemetry ingestion core.
 * @details  : Coordinates UDP reception, decoding, and state publication.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include "GeoTransform.hpp"
#include "SeqLock.hpp"
#include "TelemetryTypes.hpp"
#include "UdpTelemetryEndpoint.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace aerotwin {

/*!
    \class TelemetryIngestCore

    Transport-agnostic vehicle state aggregator.

    * MAVLink: owns a std::jthread that blocks on a UDP socket and decodes
      datagrams in place. No allocation happens in the loop - the receive
      buffer is owned by the thread and handed to the decoder as a std::span.
    * ROS 2 / Zenoh: their own executors call publish() (or submitDatagram())
      straight from their callback threads. Nothing here assumes the Qt event
      loop exists.

    Readers call snapshot(), which is wait-free. The write side is serialised
    by a small mutex so multiple transports can feed one seqlock slot without
    violating its single-writer contract.
*/
class TelemetryIngestCore
{
public:
    /*! Folds a datagram into \a state; returns true when anything changed. */
    using Decoder = std::function<bool(std::span<const std::byte>, VehicleState &)>;

    /** @brief Creates an aggregator with MAVLink decoding enabled. */
    TelemetryIngestCore();
    /** @brief Stops the ingest worker before destruction. */
    ~TelemetryIngestCore();

    TelemetryIngestCore(const TelemetryIngestCore &) = delete;
    TelemetryIngestCore &operator=(const TelemetryIngestCore &) = delete;

    /*! Defaults to the built-in MAVLink v2 decoder. */
    void setDecoder(Decoder decoder);

    /*! Pins the ENU tangent plane. Without it, positions fall back to NED odometry. */
    void setOrigin(const GeoPoint &origin);
    /** @brief Reports whether an ENU origin is configured. @return True when available. */
    [[nodiscard]] bool hasOrigin() const;
    /** @brief Returns the ENU origin. @return Current geographic origin. */
    [[nodiscard]] GeoPoint origin() const;

    /*! true when the origin should latch onto the first valid 3D fix. */
    void setAutoOrigin(bool enabled) noexcept { m_autoOrigin.store(enabled, std::memory_order_relaxed); }

    /** @brief Starts the UDP ingest worker. @param udpPort Local listening port. @param error Optional failure text. @return True on success. */
    bool start(std::uint16_t udpPort, std::string *error = nullptr);
    /** @brief Requests worker shutdown and closes its socket. */
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return m_running.load(std::memory_order_acquire); }

    /*! ROS 2 / Zenoh entry point - safe from any thread. */
    void publish(const VehicleState &state);

    /*! Feed a raw frame from an alternative transport (serial, TCP, replay). */
    bool submitDatagram(std::span<const std::byte> datagram);

    [[nodiscard]] VehicleState snapshot() const { return m_slot.load(); }
    [[nodiscard]] std::uint32_t generation() const noexcept { return m_slot.generation(); }

    /*! Local ENU position derived from the pinned origin, metres. */
    [[nodiscard]] Vec3 enuPosition(const VehicleState &state) const;

    [[nodiscard]] std::uint64_t datagramsReceived() const noexcept
    {
        return m_datagrams.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t decodeFailures() const noexcept
    {
        return m_decodeFailures.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t lastPacketMonotonicUs() const noexcept
    {
        return m_lastPacketUs.load(std::memory_order_relaxed);
    }

private:
    /** @brief Executes the receive loop. @param stopToken Cooperative cancellation token. @param port Bound port. */
    void run(std::stop_token stopToken, std::uint16_t port);
    /** @brief Publishes an accumulated vehicle snapshot. @param state State to commit. */
    void commit(const VehicleState &state);

    mutable std::mutex m_writeMutex;   // serialises writers, never taken by readers
    mutable std::mutex m_originMutex;
    GeoTransform m_transform;

    SeqLockSlot<VehicleState> m_slot;
    VehicleState m_accumulator{};      // guarded by m_writeMutex

    Decoder m_decoder;
    UdpTelemetryEndpoint m_endpoint;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_autoOrigin{true};
    std::atomic<std::uint64_t> m_datagrams{0};
    std::atomic<std::uint64_t> m_decodeFailures{0};
    std::atomic<std::uint64_t> m_lastPacketUs{0};

    std::jthread m_worker;
};

} // namespace aerotwin
