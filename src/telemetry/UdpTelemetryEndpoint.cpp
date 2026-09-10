/**
 * @file     : UdpTelemetryEndpoint.cpp
 * @brief    : Implements the UDP telemetry receiver endpoint.
 * @details  : Opens, reads, and closes the MAVLink UDP socket.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "UdpTelemetryEndpoint.hpp"

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

#include <cerrno>
#include <cstring>
#include <mutex>

namespace aerotwin {

namespace {

#if defined(_WIN32)
/** @brief Initializes the process-wide Winsock runtime exactly once. */
void ensureWinsock()
{
    // One-time init; Winsock refcounts, but a single startup keeps teardown simple.
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA data{};
        ::WSAStartup(MAKEWORD(2, 2), &data);
    });
}

/**
 * @brief Formats the most recent Winsock error.
 * @return Human-readable socket error string.
 */
std::string lastSocketError()
{
    return "winsock error " + std::to_string(::WSAGetLastError());
}
#else
/**
 * @brief Formats the most recent POSIX socket error.
 * @return Human-readable socket error string.
 */
std::string lastSocketError()
{
    return std::string(std::strerror(errno));
}
#endif

} // namespace

/** @brief Closes the socket when the endpoint is destroyed. */
UdpTelemetryEndpoint::~UdpTelemetryEndpoint()
{
    close();
}

/**
 * @brief Opens a UDP socket bound to all network interfaces.
 * @param port Local UDP port to bind.
 * @param error Optional destination for a human-readable socket error.
 * @return True when the endpoint is ready to receive datagrams.
 */
bool UdpTelemetryEndpoint::open(std::uint16_t port, std::string *error)
{
    close();

#if defined(_WIN32)
    ensureWinsock();
#endif

    const auto handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#if defined(_WIN32)
    if (handle == INVALID_SOCKET) {
#else
    if (handle < 0) {
#endif
        if (error)
            *error = "socket(): " + lastSocketError();
        return false;
    }

    int reuse = 1;
    ::setsockopt(handle, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char *>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(handle, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        if (error)
            *error = "bind(" + std::to_string(port) + "): " + lastSocketError();
#if defined(_WIN32)
        ::closesocket(handle);
#else
        ::close(handle);
#endif
        return false;
    }

    m_socket = SocketHandle(handle);
    return true;
}

/** @brief Closes the active socket, if any, and marks the endpoint unavailable. */
void UdpTelemetryEndpoint::close()
{
    if (m_socket == InvalidSocket)
        return;
#if defined(_WIN32)
    ::closesocket(::SOCKET(m_socket));
#else
    ::close(m_socket);
#endif
    m_socket = InvalidSocket;
}

/**
 * @brief Waits for one UDP datagram and returns its view into caller-owned storage.
 * @param buffer Storage populated by the socket receive operation.
 * @param timeout Maximum time spent waiting for readable data.
 * @return Received datagram bytes, or an empty span on timeout, error, or invalid input.
 */
std::span<const std::byte> UdpTelemetryEndpoint::receive(std::span<std::byte> buffer,
                                                        std::chrono::milliseconds timeout)
{
    if (m_socket == InvalidSocket || buffer.empty())
        return {};

#if defined(_WIN32)
    const auto handle = ::SOCKET(m_socket);
#else
    const auto handle = m_socket;
#endif

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(handle, &readSet);

    timeval tv{};
    tv.tv_sec = long(timeout.count() / 1000);
    tv.tv_usec = long((timeout.count() % 1000) * 1000);

    const int ready = ::select(int(handle) + 1, &readSet, nullptr, nullptr, &tv);
    if (ready <= 0)
        return {}; // timeout or error: caller re-checks its stop token

    const auto received = ::recvfrom(handle,
                                     reinterpret_cast<char *>(buffer.data()),
                                     int(buffer.size()),
                                     0,
                                     nullptr,
                                     nullptr);
    if (received <= 0)
        return {};

    return buffer.first(std::size_t(received));
}

} // namespace aerotwin
