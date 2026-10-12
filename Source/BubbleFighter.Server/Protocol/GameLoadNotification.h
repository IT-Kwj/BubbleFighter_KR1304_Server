#pragma once

#include "TcpPacketCodec.h"
#include <array>
#include <map>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // E068/E069: 0x009D9930 -> 0x010D1D70 -> 0x009E5E70 -> game stage 5.
    // GameStage then runs internal loading steps; stage 5 is not load completion.
    // At<number> denotes a CLIENT OBJECT offset, never wire padding.
    // Unknown values are preserved; zero initialization is not a valid start policy.
    struct GameLoadNotification
    {
        static constexpr std::uint16_t MessageId = 510;
        static constexpr std::size_t MaxPayloadBytes = 65536;
        static constexpr std::size_t MaxCollectionEntries = 1024;
        static constexpr std::size_t MaxTotalCollectionEntries = 1024;
        std::array<std::u16string, 3> TextsAt0;
        std::uint32_t ValueAt72 = 0;
        std::uint32_t ValueAt76 = 0;
        std::uint8_t SignedByteAt80 = 0; // Original reader sign-extends to int32.
        std::uint8_t FlagsAt84 = 0;
        std::uint32_t ValueAt88 = 0;
        std::uint8_t ByteAt92 = 0;
        // Original readers sign-extend the byte keys to int32 before tree insertion.
        std::map<std::int8_t, std::array<std::uint32_t, 2>> EntriesAt96;
        std::map<std::uint32_t, std::array<std::uint32_t, 2>> EntriesAt104;
        std::map<std::int8_t, std::array<std::uint32_t, 2>> EntriesAt112;

        TcpPacket Encode() const;
        static GameLoadNotification Decode(const TcpPacket& packet);
        bool operator==(const GameLoadNotification&) const = default;
    };
}
