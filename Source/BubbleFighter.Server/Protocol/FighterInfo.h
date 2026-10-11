#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    class PacketReader;
    class PacketWriter;

    struct FighterInfoOptionalFields
    {
        std::uint64_t WideValue = 0;
        std::array<std::uint32_t, 2> Values{};
        std::uint32_t BooleanValue = 0;
        bool operator==(const FighterInfoOptionalFields&) const = default;
    };

    struct SocialSystemInfo
    {
        std::array<std::uint32_t, 7> Values{};
        std::uint32_t BooleanValue = 0;
        std::uint64_t WideValue = 0;
        std::array<std::uint32_t, 2> BooleanValues{};
        bool operator==(const SocialSystemInfo&) const = default;
    };

    struct FighterMapRankEntry
    {
        std::uint32_t Key = 0;
        std::array<std::uint32_t, 2> Values{};
        bool operator==(const FighterMapRankEntry&) const = default;
    };

    struct FighterSkillSlotEntry
    {
        std::uint32_t Key = 0;
        std::array<std::uint32_t, 5> Values{};
        bool operator==(const FighterSkillSlotEntry&) const = default;
    };

    struct FighterSkillSlotGroup
    {
        std::uint32_t Key = 0;
        std::vector<FighterSkillSlotEntry> Entries;
        bool operator==(const FighterSkillSlotGroup&) const = default;
    };

    struct FighterAbilityEntry
    {
        std::uint16_t Key = 0;
        std::uint32_t Value = 0;
        bool operator==(const FighterAbilityEntry&) const = default;
    };

    struct FighterInfoGroupedEntry
    {
        std::uint16_t FirstValue = 0;
        std::uint16_t SecondValue = 0;
        std::uint32_t ThirdValue = 0;
        bool operator==(const FighterInfoGroupedEntry&) const = default;
    };

    struct FighterInfoGroup
    {
        std::uint16_t Key = 0;
        std::vector<FighterInfoGroupedEntry> Entries;
        bool operator==(const FighterInfoGroup&) const = default;
    };

    // E026: complete BFCLIENT02 wire layout; most business meanings remain unknown.
    // At<number> refers to a CLIENT OBJECT offset, never a network offset.
    // Collections preserve wire order and keys, including noncanonical map input.
    struct FighterInfo
    {
        static constexpr std::uint8_t PresentMarker = 0x3A;
        static constexpr std::uint8_t AbsentMarker = 0xBA;
        // Local resource limits, not established limits of the original server.
        static constexpr std::uint32_t MaxCollectionEntries = 1024;
        static constexpr std::uint32_t MaxTotalCollectionEntries = 4096;

        std::uint32_t ValueAt4 = 0;
        std::u16string TextAt8;
        // E051: index 1 (+36) is level. Other indices remain offset-named.
        std::array<std::uint32_t, 3> ValuesAt32{};
        std::uint8_t MarkerAt48 = AbsentMarker;
        std::optional<FighterInfoOptionalFields> OptionalAt48;
        std::array<std::uint32_t, 54> ValuesAt80{};
        std::vector<std::array<std::uint32_t, 5>> EntriesAt304;
        std::array<std::uint32_t, 2> ValuesAt296{};
        std::uint32_t ValueAt316 = 0;
        std::uint8_t ByteAt320 = 0;
        std::uint32_t ValueAt324 = 0;
        std::uint8_t MarkerAt328 = AbsentMarker;
        std::optional<std::uint8_t> OptionalAt328;
        std::uint32_t ValueAt332 = 0;
        std::uint8_t ByteAt336 = 0;
        std::uint32_t ValueAt340 = 0;
        SocialSystemInfo Social;
        std::array<std::uint32_t, 8> ValuesAt404{};
        std::vector<FighterMapRankEntry> Area32Ranks;
        std::array<std::uint32_t, 3> ValuesAt444{};
        std::vector<FighterMapRankEntry> ZombieEscapeRanks;
        std::u16string TextAt504;
        std::uint32_t ValueAt400 = 0;
        std::array<std::uint32_t, 10> ValuesAt464{};
        std::vector<FighterSkillSlotGroup> SkillSlots;
        std::vector<FighterAbilityEntry> Abilities;
        std::array<std::uint32_t, 3> ValuesAt552{};
        std::vector<FighterInfoGroup> GroupsAt564;

        void Write(PacketWriter& writer) const;
        static FighterInfo Read(PacketReader& reader);
        bool operator==(const FighterInfo&) const = default;
    };
}
