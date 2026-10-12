#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E082: 0x009E24B5 reads three uint32 values and applies nonzero tests.
    // These are transport choices, not authentication or gameplay authority.
    struct GameTransportOptionsNotification
    {
        static constexpr std::uint16_t MessageId = 585;
        std::uint32_t RelayEnabledValue = 0;
        std::uint32_t UdpEnabledValue = 0;
        std::uint32_t MovementDeltaEnabledValue = 0;

        TcpPacket Encode() const;
        static GameTransportOptionsNotification Decode(const TcpPacket& packet);
        bool operator==(const GameTransportOptionsNotification&) const = default;
    };
}
