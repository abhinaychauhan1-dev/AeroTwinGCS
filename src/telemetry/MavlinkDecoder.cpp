/**
 * @file     : MavlinkDecoder.cpp
 * @brief    : Implements MAVLink telemetry message decoding.
 * @details  : Extracts drone state updates from MAVLink packets.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include "MavlinkDecoder.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace aerotwin::mavlink {

namespace {

constexpr std::size_t MaxPayload = 255;

/**
 * @brief Updates a MAVLink X.25 checksum with one byte.
 * @param data Byte to incorporate.
 * @param crc Checksum updated in place.
 */
void crcAccumulate(std::uint8_t data, std::uint16_t &crc) noexcept
{
    std::uint8_t tmp = data ^ std::uint8_t(crc & 0xFF);
    tmp ^= std::uint8_t(tmp << 4);
    crc = std::uint16_t((crc >> 8) ^ (std::uint16_t(tmp) << 8) ^ (std::uint16_t(tmp) << 3)
                        ^ (std::uint16_t(tmp) >> 4));
}

/**
 * @brief Reads a little-endian scalar from a packet payload.
 * @tparam T Scalar type to read.
 * @param base First payload byte.
 * @param offset Byte offset of the scalar.
 * @return Decoded scalar value.
 */
template <typename T>
[[nodiscard]] T readLe(const std::byte *base, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, base + offset, sizeof(T));
    return value; // MAVLink is little-endian; every supported target is too.
}

} // namespace

/**
 * @brief Calculates a MAVLink X.25 checksum over a byte sequence.
 * @param data Bytes included in the checksum.
 * @param seed Initial checksum value.
 * @return Calculated 16-bit checksum.
 */
std::uint16_t crcCalculate(std::span<const std::byte> data, std::uint16_t seed) noexcept
{
    std::uint16_t crc = seed;
    for (std::byte b : data)
        crcAccumulate(std::to_integer<std::uint8_t>(b), crc);
    return crc;
}

/**
 * @brief Returns the message-specific CRC extra byte for supported MAVLink IDs.
 * @param messageId MAVLink message identifier.
 * @return CRC extra byte, or zero when the message is unsupported.
 */
std::uint8_t crcExtra(std::uint32_t messageId) noexcept
{
    switch (messageId) {
    case Heartbeat: return 50;
    case SysStatus: return 124;
    case GpsRawInt: return 24;
    case AttitudeQuaternion: return 246;
    case GlobalPositionInt: return 104;
    default: return 0;
    }
}

/**
 * @brief Validates and exposes a MAVLink v2 frame without copying its payload.
 * @param datagram Raw UDP datagram containing a MAVLink frame.
 * @return Parsed frame, or no value for malformed, truncated, or invalid frames.
 */
std::optional<Frame> parseV2(std::span<const std::byte> datagram) noexcept
{
    if (datagram.size() < HeaderLength + ChecksumLength || datagram[0] != MagicV2)
        return std::nullopt;

    const auto *raw = datagram.data();
    const auto payloadLength = std::to_integer<std::size_t>(datagram[1]);
    const auto incompatFlags = std::to_integer<std::uint8_t>(datagram[2]);

    const std::size_t signatureLength = (incompatFlags & IncompatFlagSigned) ? SignatureLength : 0;
    const std::size_t frameLength = HeaderLength + payloadLength + ChecksumLength + signatureLength;
    if (datagram.size() < frameLength)
        return std::nullopt;

    Frame frame;
    frame.sequence = std::to_integer<std::uint8_t>(datagram[4]);
    frame.systemId = std::to_integer<std::uint8_t>(datagram[5]);
    frame.componentId = std::to_integer<std::uint8_t>(datagram[6]);
    frame.messageId = std::uint32_t(std::to_integer<std::uint8_t>(datagram[7]))
                    | (std::uint32_t(std::to_integer<std::uint8_t>(datagram[8])) << 8)
                    | (std::uint32_t(std::to_integer<std::uint8_t>(datagram[9])) << 16);
    frame.payload = datagram.subspan(HeaderLength, payloadLength);

    if (const std::uint8_t extra = crcExtra(frame.messageId); extra != 0) {
        // CRC spans everything after the magic byte up to the checksum, then
        // the message-specific CRC_EXTRA byte.
        std::uint16_t crc = crcCalculate(datagram.subspan(1, HeaderLength - 1 + payloadLength));
        crcAccumulate(extra, crc);

        const auto received = std::uint16_t(
            std::to_integer<std::uint16_t>(raw[HeaderLength + payloadLength])
            | (std::to_integer<std::uint16_t>(raw[HeaderLength + payloadLength + 1]) << 8));
        if (crc != received)
            return std::nullopt;
    }

    return frame;
}

/**
 * @brief Applies supported MAVLink message fields to an accumulated vehicle state.
 * @param frame Validated MAVLink frame to decode.
 * @param state Vehicle state updated in place.
 * @return True when the message type is supported and applied.
 */
bool applyFrame(const Frame &frame, VehicleState &state) noexcept
{
    // v2 truncates trailing zero bytes, so decode from a zero-extended copy.
    std::array<std::byte, MaxPayload> payload{};
    const std::size_t copied = std::min(frame.payload.size(), payload.size());
    std::memcpy(payload.data(), frame.payload.data(), copied);
    const std::byte *p = payload.data();

    state.systemId = frame.systemId;
    state.source = TelemetrySource::Mavlink;

    switch (frame.messageId) {
    case Heartbeat: {
        state.customMode = readLe<std::uint32_t>(p, 0);
        const auto baseMode = std::to_integer<std::uint8_t>(p[6]);
        state.armed = (baseMode & 0x80) != 0; // MAV_MODE_FLAG_SAFETY_ARMED
        return true;
    }

    case SysStatus: {
        state.batteryVolts = float(readLe<std::uint16_t>(p, 14)) / 1000.0f;
        const auto remaining = readLe<std::int8_t>(p, 30);
        state.batteryPercent = remaining < 0 ? 0.0f : float(remaining);
        return true;
    }

    case GpsRawInt: {
        const auto fixType = std::to_integer<std::uint8_t>(p[28]);
        state.fix = fixType >= 6   ? GpsFixType::RtkFixed
                  : fixType == 5   ? GpsFixType::RtkFloat
                  : fixType == 4   ? GpsFixType::Dgps
                  : fixType == 3   ? GpsFixType::Fix3D
                  : fixType == 2   ? GpsFixType::Fix2D
                                   : GpsFixType::NoFix;
        state.satellites = std::to_integer<std::uint8_t>(p[29]);
        return true;
    }

    case GlobalPositionInt: {
        state.global.latitudeDeg = double(readLe<std::int32_t>(p, 4)) * 1e-7;
        state.global.longitudeDeg = double(readLe<std::int32_t>(p, 8)) * 1e-7;
        state.global.altitudeAmsl = double(readLe<std::int32_t>(p, 12)) * 1e-3;
        state.velocityNed = {double(readLe<std::int16_t>(p, 20)) * 1e-2,
                             double(readLe<std::int16_t>(p, 22)) * 1e-2,
                             double(readLe<std::int16_t>(p, 24)) * 1e-2};
        state.hasGlobalFix = true;
        return true;
    }

    case AttitudeQuaternion: {
        state.orientation = {double(readLe<float>(p, 4)),
                             double(readLe<float>(p, 8)),
                             double(readLe<float>(p, 12)),
                             double(readLe<float>(p, 16))};
        return true;
    }

    default:
        return false;
    }
}

/**
 * @brief Parses and applies one MAVLink v2 datagram.
 * @param datagram Raw MAVLink datagram bytes.
 * @param state Vehicle state updated when decoding succeeds.
 * @return True when a supported frame is decoded successfully.
 */
bool decodeDatagram(std::span<const std::byte> datagram, VehicleState &state) noexcept
{
    const auto frame = parseV2(datagram);
    if (!frame)
        return false;
    return applyFrame(*frame, state);
}

} // namespace aerotwin::mavlink
