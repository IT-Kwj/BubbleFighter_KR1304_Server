#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    class PacketReader;
    class PacketWriter;

    struct SessionTextEntry
    {
        std::uint32_t Value = 0;
        std::u16string Text;
        bool operator==(const SessionTextEntry&) const = default;
    };

    struct SessionValueGroup
    {
        std::uint16_t Key = 0;
        std::vector<std::uint32_t> Values;
        bool operator==(const SessionValueGroup&) const = default;
    };

    // E045/E047: At<number> names CLIENT OBJECT offsets, not wire offsets.
    // Boolean fields retain their uint32 wire representation; the client
    // normalizes nonzero values to true. Defaults below are codec defaults,
    // not a server-approved room/plaza configuration.
    struct SessionConfiguration
    {
        std::uint32_t ValueAt0 = 0;
        std::uint16_t ShortAt4 = 0;
        std::uint32_t BooleanAt8 = 0;
        std::array<std::uint32_t, 6> ValuesAt20{};
        std::array<std::u16string, 4> TextsAt44;
        std::uint32_t ValueAt140 = 0;
        std::array<std::u16string, 6> TextsAt144;
        std::array<std::uint32_t, 2> ValuesAt288{};
        std::uint32_t BooleanAt296 = 0;
        std::vector<std::array<std::uint32_t, 2>> PairsAt297;
        std::uint32_t BooleanAt309 = 0;
        std::vector<SessionTextEntry> EntriesAt310;
        std::array<std::uint32_t, 5> BooleansAt322{};
        std::vector<SessionValueGroup> GroupsAt328;
        std::array<std::uint32_t, 2> BooleansAt340{};
        std::uint8_t ByteAt344 = 0;
        std::array<std::uint32_t, 2> ValuesAt348{};
        std::array<std::u16string, 2> TextsAt356;
        std::uint32_t ValueAt404 = 0;
        bool operator==(const SessionConfiguration&) const = default;
    };

    // Shared descriptor prefix of S117. This is not the full notification:
    // its members, maps, team data and trailing fields must follow separately.
    struct SessionDescriptor
    {
        static constexpr std::uint32_t MaxCollectionEntries = 1024;
        static constexpr std::uint32_t MaxTotalCollectionEntries = 4096;

        std::uint16_t ShortAt0 = 0;
        std::uint32_t ValueAt4 = 0;
        std::u16string TextAt8;
        std::array<std::uint16_t, 3> ShortsAt32{};
        std::uint32_t BooleanAt38 = 0;
        std::u16string TextAt40;
        // Handler 0x009AC490: 1 -> stage 2, 2 -> stage 4,
        // 3 -> stage 3 (plaza), 6 -> stage 15. Unknown values are preserved.
        std::uint32_t SessionType = 0;
        SessionConfiguration Configuration;
        std::uint32_t ValueAt68 = 0;
        std::array<std::uint32_t, 2> FlagsAt72{};
        std::uint64_t WideAt80 = 0;
        std::u16string TextAt88;
        std::uint32_t BooleanAt112 = 0;
        std::array<std::uint32_t, 3> ValuesAt116{};
        std::uint16_t ShortAt128 = 0;

        void Write(PacketWriter& writer) const;
        static SessionDescriptor Read(PacketReader& reader);
        bool operator==(const SessionDescriptor&) const = default;
    };
}
