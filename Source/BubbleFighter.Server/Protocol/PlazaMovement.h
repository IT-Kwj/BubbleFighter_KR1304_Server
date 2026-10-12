#pragma once
#include "TcpPacketCodec.h"
#include <array>
#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 913640 writes C511: 4+12+1+1+4 bytes. 9D9930
    // case 514 reads the same fields preceded by the trusted player ID.
    struct PlazaMovementRequest
    {
        static constexpr std::uint16_t MessageId = 511;
        std::uint32_t Sequence = 0;
        std::array<std::uint32_t, 3> PositionBits{};
        std::array<std::uint8_t, 2> Axes{};
        std::uint32_t YawBits = 0;
        void Validate() const;
        TcpPacket Encode() const;
        static PlazaMovementRequest Decode(const TcpPacket& packet);
        bool operator==(const PlazaMovementRequest&) const = default;
    };
    struct PlazaMovementNotification
    {
        static constexpr std::uint16_t MessageId = 514;
        std::uint32_t PlayerId = 0;
        PlazaMovementRequest Movement;
        TcpPacket Encode() const;
        static PlazaMovementNotification Decode(const TcpPacket& packet);
        bool operator==(const PlazaMovementNotification&) const = default;
    };
}
