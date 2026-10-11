#include "FighterInfo.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        template <std::size_t Size>
        void WriteValues(PacketWriter& writer, const std::array<std::uint32_t, Size>& values)
        {
            for (const auto value : values) { writer.WriteUInt32(value); }
        }

        template <std::size_t Size>
        void ReadValues(PacketReader& reader, std::array<std::uint32_t, Size>& values)
        {
            for (auto& value : values) { value = reader.ReadUInt32(); }
        }

        void CheckCount(std::size_t count, std::size_t& remaining)
        {
            if (count > FighterInfo::MaxCollectionEntries || count > remaining)
            {
                throw std::length_error("Fighter info exceeds supported collection limit.");
            }
            remaining -= count;
        }

        template <typename Entry, typename WriteEntry>
        void WriteEntries(PacketWriter& writer, const std::vector<Entry>& entries,
            std::size_t& remaining, WriteEntry writeEntry)
        {
            CheckCount(entries.size(), remaining);
            writer.WriteUInt32(static_cast<std::uint32_t>(entries.size()));
            for (const auto& entry : entries) { writeEntry(entry); }
        }

        template <typename Entry, typename ReadEntry>
        std::vector<Entry> ReadEntries(PacketReader& reader, std::size_t& remaining,
            std::size_t minimumBytes, ReadEntry readEntry)
        {
            const auto count = reader.ReadUInt32();
            CheckCount(count, remaining);
            if (count > reader.RemainingBytes() / minimumBytes)
            {
                throw std::runtime_error("Truncated fighter info collection.");
            }
            std::vector<Entry> entries;
            entries.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index) { entries.push_back(readEntry()); }
            return entries;
        }
    }

    void FighterInfo::Write(PacketWriter& writer) const
    {
        if ((MarkerAt48 == PresentMarker) != OptionalAt48.has_value() ||
            (MarkerAt328 == PresentMarker) != OptionalAt328.has_value())
        {
            throw std::invalid_argument("Fighter info optional fields do not match their markers.");
        }
        std::size_t remaining = MaxTotalCollectionEntries;
        writer.WriteUInt32(ValueAt4);
        writer.WriteUtf16(TextAt8);
        WriteValues(writer, ValuesAt32);
        writer.WriteUInt8(MarkerAt48);
        if (OptionalAt48)
        {
            writer.WriteUInt64(OptionalAt48->WideValue);
            WriteValues(writer, OptionalAt48->Values);
            writer.WriteUInt32(OptionalAt48->BooleanValue);
        }
        WriteValues(writer, ValuesAt80);
        WriteEntries(writer, EntriesAt304, remaining, [&](const auto& entry) { WriteValues(writer, entry); });
        WriteValues(writer, ValuesAt296);
        writer.WriteUInt32(ValueAt316);
        writer.WriteUInt8(ByteAt320);
        writer.WriteUInt32(ValueAt324);
        writer.WriteUInt8(MarkerAt328);
        if (OptionalAt328) { writer.WriteUInt8(*OptionalAt328); }
        writer.WriteUInt32(ValueAt332);
        writer.WriteUInt8(ByteAt336);
        writer.WriteUInt32(ValueAt340);
        WriteValues(writer, Social.Values);
        writer.WriteUInt32(Social.BooleanValue);
        writer.WriteUInt64(Social.WideValue);
        WriteValues(writer, Social.BooleanValues);
        WriteValues(writer, ValuesAt404);
        const auto writeRank = [&](const FighterMapRankEntry& entry)
        {
            writer.WriteUInt32(entry.Key);
            WriteValues(writer, entry.Values);
        };
        WriteEntries(writer, Area32Ranks, remaining, writeRank);
        WriteValues(writer, ValuesAt444);
        WriteEntries(writer, ZombieEscapeRanks, remaining, writeRank);
        writer.WriteUtf16(TextAt504);
        writer.WriteUInt32(ValueAt400);
        // Original wire order swaps object offsets +468 and +472.
        writer.WriteUInt32(ValuesAt464[0]);
        writer.WriteUInt32(ValuesAt464[2]);
        writer.WriteUInt32(ValuesAt464[1]);
        for (std::size_t index = 3; index < ValuesAt464.size(); ++index) { writer.WriteUInt32(ValuesAt464[index]); }
        WriteEntries(writer, SkillSlots, remaining, [&](const FighterSkillSlotGroup& group)
        {
            writer.WriteUInt32(group.Key);
            WriteEntries(writer, group.Entries, remaining, [&](const FighterSkillSlotEntry& entry)
            {
                writer.WriteUInt32(entry.Key);
                WriteValues(writer, entry.Values);
            });
        });
        WriteEntries(writer, Abilities, remaining, [&](const FighterAbilityEntry& entry)
        {
            writer.WriteUInt16(entry.Key);
            writer.WriteUInt32(entry.Value);
        });
        WriteValues(writer, ValuesAt552);
        WriteEntries(writer, GroupsAt564, remaining, [&](const FighterInfoGroup& group)
        {
            writer.WriteUInt16(group.Key);
            WriteEntries(writer, group.Entries, remaining, [&](const FighterInfoGroupedEntry& entry)
            {
                writer.WriteUInt16(entry.FirstValue);
                writer.WriteUInt16(entry.SecondValue);
                writer.WriteUInt32(entry.ThirdValue);
            });
        });
    }

    FighterInfo FighterInfo::Read(PacketReader& reader)
    {
        FighterInfo value;
        std::size_t remaining = MaxTotalCollectionEntries;
        value.ValueAt4 = reader.ReadUInt32();
        value.TextAt8 = reader.ReadUtf16();
        ReadValues(reader, value.ValuesAt32);
        value.MarkerAt48 = reader.ReadUInt8();
        if (value.MarkerAt48 == PresentMarker)
        {
            value.OptionalAt48.emplace();
            value.OptionalAt48->WideValue = reader.ReadUInt64();
            ReadValues(reader, value.OptionalAt48->Values);
            value.OptionalAt48->BooleanValue = reader.ReadUInt32();
        }
        ReadValues(reader, value.ValuesAt80);
        value.EntriesAt304 = ReadEntries<std::array<std::uint32_t, 5>>(reader, remaining, 20, [&]
        {
            std::array<std::uint32_t, 5> entry{};
            ReadValues(reader, entry);
            return entry;
        });
        ReadValues(reader, value.ValuesAt296);
        value.ValueAt316 = reader.ReadUInt32();
        value.ByteAt320 = reader.ReadUInt8();
        value.ValueAt324 = reader.ReadUInt32();
        value.MarkerAt328 = reader.ReadUInt8();
        if (value.MarkerAt328 == PresentMarker) { value.OptionalAt328 = reader.ReadUInt8(); }
        value.ValueAt332 = reader.ReadUInt32();
        value.ByteAt336 = reader.ReadUInt8();
        value.ValueAt340 = reader.ReadUInt32();
        ReadValues(reader, value.Social.Values);
        value.Social.BooleanValue = reader.ReadUInt32();
        value.Social.WideValue = reader.ReadUInt64();
        ReadValues(reader, value.Social.BooleanValues);
        ReadValues(reader, value.ValuesAt404);
        const auto readRank = [&]
        {
            FighterMapRankEntry entry;
            entry.Key = reader.ReadUInt32();
            ReadValues(reader, entry.Values);
            return entry;
        };
        value.Area32Ranks = ReadEntries<FighterMapRankEntry>(reader, remaining, 12, readRank);
        ReadValues(reader, value.ValuesAt444);
        value.ZombieEscapeRanks = ReadEntries<FighterMapRankEntry>(reader, remaining, 12, readRank);
        value.TextAt504 = reader.ReadUtf16();
        value.ValueAt400 = reader.ReadUInt32();
        value.ValuesAt464[0] = reader.ReadUInt32();
        value.ValuesAt464[2] = reader.ReadUInt32();
        value.ValuesAt464[1] = reader.ReadUInt32();
        for (std::size_t index = 3; index < value.ValuesAt464.size(); ++index) { value.ValuesAt464[index] = reader.ReadUInt32(); }
        value.SkillSlots = ReadEntries<FighterSkillSlotGroup>(reader, remaining, 8, [&]
        {
            FighterSkillSlotGroup group;
            group.Key = reader.ReadUInt32();
            group.Entries = ReadEntries<FighterSkillSlotEntry>(reader, remaining, 24, [&]
            {
                FighterSkillSlotEntry entry;
                entry.Key = reader.ReadUInt32();
                ReadValues(reader, entry.Values);
                return entry;
            });
            return group;
        });
        value.Abilities = ReadEntries<FighterAbilityEntry>(reader, remaining, 6, [&]
        {
            FighterAbilityEntry entry;
            entry.Key = reader.ReadUInt16();
            entry.Value = reader.ReadUInt32();
            return entry;
        });
        ReadValues(reader, value.ValuesAt552);
        value.GroupsAt564 = ReadEntries<FighterInfoGroup>(reader, remaining, 6, [&]
        {
            FighterInfoGroup group;
            group.Key = reader.ReadUInt16();
            group.Entries = ReadEntries<FighterInfoGroupedEntry>(reader, remaining, 8, [&]
            {
                FighterInfoGroupedEntry entry;
                entry.FirstValue = reader.ReadUInt16();
                entry.SecondValue = reader.ReadUInt16();
                entry.ThirdValue = reader.ReadUInt32();
                return entry;
            });
            return group;
        });
        return value;
    }
}
