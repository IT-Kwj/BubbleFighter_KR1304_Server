#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    struct TcpPacket
    {
        std::uint16_t MessageId = 0;
        std::vector<std::uint8_t> Payload;
    };

    enum class PacketDecodeStatus
    {
        Complete,
        NeedMoreData,
        InvalidHeaderChecksum,
        InvalidPayloadChecksum,
        PayloadTooLarge
    };

    struct PacketDecodeResult
    {
        PacketDecodeStatus Status = PacketDecodeStatus::NeedMoreData;
        std::size_t FrameSize = 8;
        TcpPacket Packet;
    };

    class TcpPacketCodec
    {
    public:
        static constexpr std::size_t HeaderSize = 8;
        // A server resource policy, not an inferred original protocol limit.
        static constexpr std::uint32_t DefaultMaxPayloadSize = 1024 * 1024;

        static std::vector<std::uint8_t> Encode(
            const TcpPacket& packet,
            std::uint32_t maxPayloadSize = DefaultMaxPayloadSize);
        static PacketDecodeResult Decode(
            std::span<const std::uint8_t> bytes,
            std::uint32_t maxPayloadSize = DefaultMaxPayloadSize);
    };
}
