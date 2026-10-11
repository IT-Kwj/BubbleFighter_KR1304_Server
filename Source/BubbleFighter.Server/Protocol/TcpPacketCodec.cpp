#include "TcpPacketCodec.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        std::uint8_t XorBytes(std::span<const std::uint8_t> bytes)
        {
            std::uint8_t result = 0;
            for (const auto value : bytes)
            {
                result ^= value;
            }
            return result;
        }
    }

    std::vector<std::uint8_t> TcpPacketCodec::Encode(const TcpPacket& packet, std::uint32_t maxPayloadSize)
    {
        if (packet.Payload.size() > maxPayloadSize ||
            packet.Payload.size() > std::numeric_limits<std::uint32_t>::max() ||
            packet.Payload.size() > std::numeric_limits<std::size_t>::max() - HeaderSize)
        {
            throw std::length_error("Packet payload exceeds configured limit.");
        }

        // E005: sub_10BB3E0 writes payload length and XOR checksums.
        std::vector<std::uint8_t> bytes(HeaderSize + packet.Payload.size());
        const auto length = static_cast<std::uint32_t>(packet.Payload.size());
        for (std::size_t index = 0; index < 4; ++index)
        {
            bytes[index] = static_cast<std::uint8_t>(length >> (index * 8));
        }
        bytes[4] = static_cast<std::uint8_t>(packet.MessageId);
        bytes[5] = static_cast<std::uint8_t>(packet.MessageId >> 8);
        bytes[6] = XorBytes(packet.Payload);
        bytes[7] = XorBytes(std::span(bytes).first(7));
        std::copy(packet.Payload.begin(), packet.Payload.end(), bytes.begin() + HeaderSize);
        return bytes;
    }

    PacketDecodeResult TcpPacketCodec::Decode(std::span<const std::uint8_t> bytes, std::uint32_t maxPayloadSize)
    {
        PacketDecodeResult result;
        if (bytes.size() < HeaderSize)
        {
            return result;
        }
        if (XorBytes(bytes.first(HeaderSize)) != 0)
        {
            result.Status = PacketDecodeStatus::InvalidHeaderChecksum;
            return result;
        }

        std::uint32_t length = 0;
        for (std::size_t index = 0; index < 4; ++index)
        {
            length |= static_cast<std::uint32_t>(bytes[index]) << (index * 8);
        }
        if (length > maxPayloadSize || length > std::numeric_limits<std::size_t>::max() - HeaderSize)
        {
            result.Status = PacketDecodeStatus::PayloadTooLarge;
            return result;
        }
        result.FrameSize = HeaderSize + static_cast<std::size_t>(length);
        if (bytes.size() < result.FrameSize)
        {
            return result;
        }
        const auto payload = bytes.subspan(HeaderSize, length);
        if (XorBytes(payload) != bytes[6])
        {
            result.Status = PacketDecodeStatus::InvalidPayloadChecksum;
            return result;
        }
        result.Packet.MessageId = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[4]) | (static_cast<std::uint16_t>(bytes[5]) << 8));
        result.Packet.Payload.assign(payload.begin(), payload.end());
        result.Status = PacketDecodeStatus::Complete;
        return result;
    }
}
