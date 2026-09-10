/**
 * @file     : UdpTelemetryEndpoint.hpp
 * @brief    : Declares the UDP telemetry receiver endpoint.
 * @details  : Receives MAVLink datagrams from the drone telemetry stream.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace aerotwin {

/*!
    Minimal blocking UDP receiver.

    Deliberately not QUdpSocket: the ingest loop runs on a plain std::jthread
    with no Qt event loop, and a bare socket with a select() timeout is what
    lets the loop honour the stop_token promptly without a wakeup pipe.
*/
class UdpTelemetryEndpoint
{
public:
    /** @brief Creates an unopened UDP endpoint. */
    UdpTelemetryEndpoint() = default;
    /** @brief Closes the UDP socket during destruction. */
    ~UdpTelemetryEndpoint();

    UdpTelemetryEndpoint(const UdpTelemetryEndpoint &) = delete;
    UdpTelemetryEndpoint &operator=(const UdpTelemetryEndpoint &) = delete;

    /*! Bind to \a port on all interfaces. Returns false and fills \a error on failure. */
    bool open(std::uint16_t port, std::string *error = nullptr);
    /** @brief Closes the active UDP socket. */
    void close();
    /** @brief Returns socket availability. @return True when the endpoint is open. */
    [[nodiscard]] bool isOpen() const noexcept { return m_socket != InvalidSocket; }

    /*!
        Wait up to \a timeout for one datagram.

        Returns the received bytes as a subspan of \a buffer - no copy, no
        allocation. An empty span means "timed out", which is the signal the
        ingest loop uses to re-check its stop token.
    */
    [[nodiscard]] std::span<const std::byte> receive(std::span<std::byte> buffer,
                                                     std::chrono::milliseconds timeout);

private:
#if defined(_WIN32)
    using SocketHandle = std::uintptr_t;
    static constexpr SocketHandle InvalidSocket = ~SocketHandle(0);
#else
    using SocketHandle = int;
    static constexpr SocketHandle InvalidSocket = -1;
#endif

    SocketHandle m_socket = InvalidSocket;
};

} // namespace aerotwin
