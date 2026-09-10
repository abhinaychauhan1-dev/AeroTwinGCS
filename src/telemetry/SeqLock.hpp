/**
 * @file     : SeqLock.hpp
 * @brief    : Declares a sequence lock for telemetry snapshots.
 * @details  : Enables low-overhead thread-safe reads of vehicle state.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <type_traits>

namespace aerotwin {

/*!
    Single-writer / many-reader seqlock slot.

    The GUI thread must never block on the network thread, and the network
    thread must never be delayed by the GUI. A seqlock gives the reader a
    wait-free retry loop and the writer an uncontended store - no mutex, no
    allocation, no priority inversion. This is the same pattern used for shared
    telemetry pages in flight software.

    Contract: exactly one writer at a time. TelemetryIngestCore serialises the
    MAVLink thread and any ROS 2 / Zenoh callbacks behind one small mutex on
    the write side, so readers stay lock-free.

    Note: like every seqlock, a torn read is observed and discarded rather than
    prevented, which is formally a data race on T. It is well-defined in
    practice for trivially copyable T on all mainstream ABIs and is the
    accepted trade-off for wait-free reads.
*/
template <typename T>
    requires std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>
class SeqLockSlot
{
public:
    /** @brief Publishes a value for lock-free readers. @param value Snapshot to store. */
    void store(const T &value) noexcept
    {
        const std::uint32_t seq = m_sequence.load(std::memory_order_relaxed);
        m_sequence.store(seq + 1, std::memory_order_relaxed); // odd: write in progress
        std::atomic_thread_fence(std::memory_order_release);

        m_value = value;

        std::atomic_thread_fence(std::memory_order_release);
        m_sequence.store(seq + 2, std::memory_order_relaxed); // even: consistent
    }

    /** @brief Reads one internally consistent snapshot. @return Published value. */
    [[nodiscard]] T load() const noexcept
    {
        T result{};
        for (unsigned spin = 0;; ++spin) {
            const std::uint32_t before = m_sequence.load(std::memory_order_acquire);
            if (before & 1u) { // writer mid-update
                backoff(spin);
                continue;
            }

            result = m_value;

            std::atomic_thread_fence(std::memory_order_acquire);
            if (m_sequence.load(std::memory_order_relaxed) == before)
                return result;

            backoff(spin);
        }
    }

    /** @brief Returns the current publication generation. @return Sequence counter. */
    [[nodiscard]] std::uint32_t generation() const noexcept
    {
        return m_sequence.load(std::memory_order_acquire);
    }

private:
    /** @brief Yields after repeated reader retries. @param spin Consecutive retry count. */
    static void backoff(unsigned spin) noexcept
    {
        if (spin > 16)
            std::this_thread::yield();
    }

    alignas(64) std::atomic<std::uint32_t> m_sequence{0};
    T m_value{};
};

} // namespace aerotwin
