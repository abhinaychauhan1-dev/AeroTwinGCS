/**
 * @file     : MavlinkDecoder.hpp
 * @brief    : Declares MAVLink telemetry decoding support.
 * @details  : Translates MAVLink messages into vehicle telemetry state.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#pragma once

#include "TelemetryTypes.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace aerotwin::mavlink {

/** @brief MAVLink v2 start-of-frame marker. */
inline constexpr std::byte MagicV2{0xFD};
/** @brief MAVLink v2 header length in bytes. */
inline constexpr std::size_t HeaderLength = 10;
/** @brief MAVLink checksum length in bytes. */
inline constexpr std::size_t ChecksumLength = 2;
/** @brief MAVLink v2 signature length in bytes. */
inline constexpr std::size_t SignatureLength = 13;
/** @brief Incompatibility flag indicating a trailing packet signature. */
inline constexpr std::uint8_t IncompatFlagSigned = 0x01;

/** @brief MAVLink message IDs decoded by this lightweight telemetry parser. */
enum MessageId : std::uint32_t {
    Heartbeat = 0,
    SysStatus = 1,
    AttitudeQuaternion = 31,
    GlobalPositionInt = 33,
    GpsRawInt = 24,
};

/*! Zero-copy view of one validated frame; \a payload aliases the input buffer. */
struct Frame
{
    std::uint32_t messageId = 0;
    std::uint8_t systemId = 0;
    std::uint8_t componentId = 0;
    std::uint8_t sequence = 0;
    std::span<const std::byte> payload;
};

/**
 * @brief Calculates the MAVLink X.25 checksum.
 * @param data Bytes to checksum.
 * @param seed Initial checksum value.
 * @return Calculated checksum.
 */
[[nodiscard]] std::uint16_t crcCalculate(std::span<const std::byte> data,
                                         std::uint16_t seed = 0xFFFF) noexcept;

/**
 * @brief Returns a supported message's MAVLink CRC extra value.
 * @param messageId MAVLink message ID.
 * @return CRC extra byte, or zero for an unknown message.
 */
[[nodiscard]] std::uint8_t crcExtra(std::uint32_t messageId) noexcept;

/*!
    @brief Parses a single MAVLink v2 frame.
    @param datagram Raw datagram bytes.
    @return Validated frame or no value for malformed input.

    Returns nullopt on bad magic, truncation or checksum failure. Payloads of
    known messages are validated; unknown message ids are returned unvalidated
    (their CRC_EXTRA is not compiled in) so callers can still route them.
*/
[[nodiscard]] std::optional<Frame> parseV2(std::span<const std::byte> datagram) noexcept;

/*!
    @brief Folds a supported frame into vehicle state.
    @param frame Parsed MAVLink frame.
    @param state Vehicle state updated in place.
    @return True when the frame type is supported.

    Handles MAVLink v2 payload truncation by zero-extending short payloads,
    which is the single most common source of garbage fields in hand-rolled
    parsers.
*/
bool applyFrame(const Frame &frame, VehicleState &state) noexcept;

/**
 * @brief Parses and applies a MAVLink datagram.
 * @param datagram Raw MAVLink datagram bytes.
 * @param state Vehicle state updated in place.
 * @return True when parsing and supported-message decoding succeed.
 */
bool decodeDatagram(std::span<const std::byte> datagram, VehicleState &state) noexcept;

} // namespace aerotwin::mavlink
