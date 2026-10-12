#pragma once
#include "TcpPacketCodec.h"
#include <array>
#include <optional>

namespace BubbleFighter::Server::Protocol
{
    // E084: 0x809B's mask controls three float32 and three byte updates.
    // The original reader ignores high mask bits; preserve them on the wire.
    // Current solo practice selects full records and does not apply deltas.
    struct GameMovementDeltaNotification
    {
        static constexpr std::uint16_t MessageId = 0x809B;
        std::uint32_t ActorObjectId = 0;
        std::uint16_t ValueAt4 = 0;
        std::uint8_t Mask = 0;
        std::array<std::optional<float>, 3> Position{};
        std::array<std::optional<std::uint8_t>, 3> AnglesAndFlags{};
        TcpPacket Encode() const;
        static GameMovementDeltaNotification Decode(const TcpPacket& packet);
        bool operator==(const GameMovementDeltaNotification&) const = default;
    };
}
