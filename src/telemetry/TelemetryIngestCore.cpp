/**
 * @file     : TelemetryIngestCore.cpp
 * @brief    : Implements the telemetry ingestion core.
 * @details  : Publishes decoded drone telemetry as safe state snapshots.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "TelemetryIngestCore.hpp"

#include "MavlinkDecoder.hpp"

#include <array>
#include <chrono>

namespace aerotwin {

namespace {

constexpr std::size_t MaxDatagram = 2048;
constexpr auto ReceiveTimeout = std::chrono::milliseconds(100);

/**
 * @brief Returns the current monotonic timestamp in microseconds.
 * @return Microseconds from an unspecified steady-clock epoch.
 */
std::uint64_t monotonicUs()
{
    using namespace std::chrono;
    return std::uint64_t(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace

/** @brief Creates an ingest core configured with the built-in MAVLink decoder. */
TelemetryIngestCore::TelemetryIngestCore()
    : m_decoder(&mavlink::decodeDatagram)
{
}

/** @brief Stops the worker before releasing ingest resources. */
TelemetryIngestCore::~TelemetryIngestCore()
{
    stop();
}

/**
 * @brief Replaces the packet decoder used by the ingest path.
 * @param decoder Decoder callback, or an empty callback to restore MAVLink decoding.
 */
void TelemetryIngestCore::setDecoder(Decoder decoder)
{
    // The worker and external publishers share the accumulator and decoder.
    std::scoped_lock lock(m_writeMutex);
    m_decoder = decoder ? std::move(decoder) : Decoder(&mavlink::decodeDatagram);
}

/**
 * @brief Sets the geographic origin used for local ENU coordinates.
 * @param origin Geodetic origin expressed as latitude, longitude, and altitude.
 */
void TelemetryIngestCore::setOrigin(const GeoPoint &origin)
{
    std::scoped_lock lock(m_originMutex);
    m_transform.setOrigin(origin);
}

/**
 * @brief Reports whether a geographic origin has been configured.
 * @return True when ENU conversion has a valid origin.
 */
bool TelemetryIngestCore::hasOrigin() const
{
    std::scoped_lock lock(m_originMutex);
    return m_transform.hasOrigin();
}

/**
 * @brief Returns the current geographic origin.
 * @return The origin stored by the coordinate transform.
 */
GeoPoint TelemetryIngestCore::origin() const
{
    std::scoped_lock lock(m_originMutex);
    return m_transform.origin();
}

/**
 * @brief Opens the UDP receiver and starts the ingest worker.
 * @param udpPort Local port on which MAVLink datagrams are received.
 * @param error Optional destination for a socket startup error.
 * @return True when the worker is already running or starts successfully.
 */
bool TelemetryIngestCore::start(std::uint16_t udpPort, std::string *error)
{
    if (isRunning())
        return true;

    if (!m_endpoint.open(udpPort, error))
        return false;

    m_running.store(true, std::memory_order_release);

    // jthread carries its own stop_token, so stop() needs no extra flag and the
    // destructor can never leak a detached socket thread.
    m_worker = std::jthread([this, udpPort](std::stop_token token) { run(std::move(token), udpPort); });
    return true;
}

/** @brief Stops the ingest worker and releases its UDP socket. */
void TelemetryIngestCore::stop()
{
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();            // bounded by ReceiveTimeout
    }
    m_endpoint.close();
    m_running.store(false, std::memory_order_release);
}

/**
 * @brief Receives and decodes packets until cancellation is requested.
 * @param stopToken Cooperative cancellation token owned by the worker thread.
 * @param udpPort Bound UDP port, retained for the worker entry-point contract.
 */
void TelemetryIngestCore::run(std::stop_token stopToken, std::uint16_t)
{
    // Owned by the thread: the hot path never touches the allocator.
    std::array<std::byte, MaxDatagram> buffer{};

    while (!stopToken.stop_requested()) {
        const std::span<const std::byte> datagram = m_endpoint.receive(buffer, ReceiveTimeout);
        if (datagram.empty())
            continue; // timed out - loop back and re-check the stop token

        m_datagrams.fetch_add(1, std::memory_order_relaxed);
        m_lastPacketUs.store(monotonicUs(), std::memory_order_relaxed);

        if (!submitDatagram(datagram))
            m_decodeFailures.fetch_add(1, std::memory_order_relaxed);
    }
}

/**
 * @brief Decodes one datagram and publishes its accumulated vehicle state.
 * @param datagram MAVLink packet bytes received from the UDP endpoint.
 * @return True when the configured decoder accepts the datagram.
 */
bool TelemetryIngestCore::submitDatagram(std::span<const std::byte> datagram)
{
    // Serialize partial-message accumulation before committing to the lock-free slot.
    std::scoped_lock lock(m_writeMutex);

    if (!m_decoder || !m_decoder(datagram, m_accumulator))
        return false;

    m_accumulator.timestampUs = monotonicUs();
    commit(m_accumulator);
    return true;
}

/**
 * @brief Publishes a complete vehicle state supplied by another transport.
 * @param state Fully populated vehicle state to make visible to readers.
 */
void TelemetryIngestCore::publish(const VehicleState &state)
{
    std::scoped_lock lock(m_writeMutex);
    m_accumulator = state;
    if (m_accumulator.timestampUs == 0)
        m_accumulator.timestampUs = monotonicUs();
    commit(m_accumulator);
}

/**
 * @brief Commits a state snapshot and initializes the automatic ENU origin.
 * @param state Latest vehicle state ready for lock-free publication.
 */
void TelemetryIngestCore::commit(const VehicleState &state)
{
    if (m_autoOrigin.load(std::memory_order_relaxed) && state.hasGlobalFix
        && state.fix >= GpsFixType::Fix3D) {
        std::scoped_lock lock(m_originMutex);
        if (!m_transform.hasOrigin())
            m_transform.setOrigin(state.global);
    }

    m_slot.store(state);
}

/**
 * @brief Converts the best available position in a vehicle state to ENU metres.
 * @param state Vehicle state containing global GNSS or local NED data.
 * @return ENU position, or a zero vector when no usable position is available.
 */
Vec3 TelemetryIngestCore::enuPosition(const VehicleState &state) const
{
    if (state.hasGlobalFix) {
        std::scoped_lock lock(m_originMutex);
        if (m_transform.hasOrigin())
            return m_transform.geodeticToEnu(state.global);
    }

    if (state.hasLocalOdometry)
        return GeoTransform::nedToEnu(state.positionNed);

    return {};
}

} // namespace aerotwin
