#pragma once
#include "GameScoreRecord.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E073: S614 reader 0x010D16A0. Full layout, no runtime reward/exit policy.
    struct GameScoreNotification
    {
        static constexpr std::uint16_t MessageId = 614;
        static constexpr std::size_t MaxPayloadBytes = GameScoreRecord::MaxPayloadBytes;
        static constexpr std::size_t MaxEntries = 1024; // Local limit, not original server policy.
        GameScoreRecord Scores;
        std::uint32_t ValueAt19180 = 0;
        std::uint32_t ElapsedMilliseconds = 0;
        std::map<std::uint32_t, std::array<std::uint32_t, 2>> EntriesAt19188;
        std::uint8_t ByteAt19196 = 0;
        std::uint8_t ByteAt19197 = 0;
        std::uint16_t FlagsAt19272 = 0;
        // Wire order is client object offsets 19200, 19248, 19224.
        std::array<std::u16string, 3> TextsAt19200;
        std::uint8_t ExtensionTag = 0;
        // Only tag 0x3A carries these three uint32 values.
        std::array<std::uint32_t, 3> ValuesAt19276{};
        TcpPacket Encode() const;
        static GameScoreNotification Decode(const TcpPacket& packet);
        bool operator==(const GameScoreNotification&) const = default;
    };
}
