#pragma once
#include "CompressedShotRecord.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct GameExtendedShotNotification
    {
        static constexpr std::uint16_t MessageId = 0x820F;
        static constexpr std::size_t MaxValues = (TcpPacketCodec::DefaultMaxPayloadSize - 40) / 4;
        CompressedShotRecord Shot;
        std::uint32_t ValueAt40 = 0;
        std::vector<std::uint32_t> ValuesAt44;
        TcpPacket Encode() const;
        static GameExtendedShotNotification Decode(const TcpPacket& packet);
        bool operator==(const GameExtendedShotNotification&) const = default;
    };
}
