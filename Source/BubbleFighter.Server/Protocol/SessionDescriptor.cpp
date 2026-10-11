#include "SessionDescriptor.h"

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

        template <std::size_t Size>
        void WriteTexts(PacketWriter& writer, const std::array<std::u16string, Size>& texts)
        {
            for (const auto& text : texts) { writer.WriteUtf16(text); }
        }

        template <std::size_t Size>
        void ReadTexts(PacketReader& reader, std::array<std::u16string, Size>& texts)
        {
            for (auto& text : texts) { text = reader.ReadUtf16(); }
        }

        void CheckCount(std::size_t count, std::size_t& remaining)
        {
            if (count > SessionDescriptor::MaxCollectionEntries || count > remaining)
            {
                throw std::length_error("Session descriptor exceeds supported collection limit.");
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
                throw std::runtime_error("Truncated session descriptor collection.");
            }
            std::vector<Entry> entries;
            entries.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index) { entries.push_back(readEntry()); }
            return entries;
        }

        void WriteConfiguration(PacketWriter& writer, const SessionConfiguration& value)
        {
            std::size_t remaining = SessionDescriptor::MaxTotalCollectionEntries;
            writer.WriteUInt32(value.ValueAt0);
            writer.WriteUInt16(value.ShortAt4);
            writer.WriteUInt32(value.BooleanAt8);
            WriteValues(writer, value.ValuesAt20);
            WriteTexts(writer, value.TextsAt44);
            writer.WriteUInt32(value.ValueAt140);
            WriteTexts(writer, value.TextsAt144);
            WriteValues(writer, value.ValuesAt288);
            writer.WriteUInt32(value.BooleanAt296);
            WriteEntries(writer, value.PairsAt297, remaining, [&](const auto& entry) { WriteValues(writer, entry); });
            writer.WriteUInt32(value.BooleanAt309);
            WriteEntries(writer, value.EntriesAt310, remaining, [&](const SessionTextEntry& entry)
            {
                writer.WriteUInt32(entry.Value);
                writer.WriteUtf16(entry.Text);
            });
            WriteValues(writer, value.BooleansAt322);
            WriteEntries(writer, value.GroupsAt328, remaining, [&](const SessionValueGroup& group)
            {
                writer.WriteUInt16(group.Key);
                WriteEntries(writer, group.Values, remaining, [&](const auto entry) { writer.WriteUInt32(entry); });
            });
            WriteValues(writer, value.BooleansAt340);
            writer.WriteUInt8(value.ByteAt344);
            WriteValues(writer, value.ValuesAt348);
            WriteTexts(writer, value.TextsAt356);
            writer.WriteUInt32(value.ValueAt404);
        }

        SessionConfiguration ReadConfiguration(PacketReader& reader)
        {
            SessionConfiguration value;
            std::size_t remaining = SessionDescriptor::MaxTotalCollectionEntries;
            value.ValueAt0 = reader.ReadUInt32();
            value.ShortAt4 = reader.ReadUInt16();
            value.BooleanAt8 = reader.ReadUInt32();
            ReadValues(reader, value.ValuesAt20);
            ReadTexts(reader, value.TextsAt44);
            value.ValueAt140 = reader.ReadUInt32();
            ReadTexts(reader, value.TextsAt144);
            ReadValues(reader, value.ValuesAt288);
            value.BooleanAt296 = reader.ReadUInt32();
            value.PairsAt297 = ReadEntries<std::array<std::uint32_t, 2>>(reader, remaining, 8, [&]
            { return std::array<std::uint32_t, 2>{reader.ReadUInt32(), reader.ReadUInt32()}; });
            value.BooleanAt309 = reader.ReadUInt32();
            value.EntriesAt310 = ReadEntries<SessionTextEntry>(reader, remaining, 8, [&]
            { return SessionTextEntry{reader.ReadUInt32(), reader.ReadUtf16()}; });
            ReadValues(reader, value.BooleansAt322);
            value.GroupsAt328 = ReadEntries<SessionValueGroup>(reader, remaining, 6, [&]
            {
                SessionValueGroup group;
                group.Key = reader.ReadUInt16();
                group.Values = ReadEntries<std::uint32_t>(reader, remaining, 4, [&] { return reader.ReadUInt32(); });
                return group;
            });
            ReadValues(reader, value.BooleansAt340);
            value.ByteAt344 = reader.ReadUInt8();
            ReadValues(reader, value.ValuesAt348);
            ReadTexts(reader, value.TextsAt356);
            value.ValueAt404 = reader.ReadUInt32();
            return value;
        }
    }

    void SessionDescriptor::Write(PacketWriter& writer) const
    {
        writer.WriteUInt16(ShortAt0);
        writer.WriteUInt32(ValueAt4);
        writer.WriteUtf16(TextAt8);
        for (const auto value : ShortsAt32) { writer.WriteUInt16(value); }
        writer.WriteUInt32(BooleanAt38);
        writer.WriteUtf16(TextAt40);
        writer.WriteUInt32(SessionType);
        WriteConfiguration(writer, Configuration);
        writer.WriteUInt32(ValueAt68);
        WriteValues(writer, FlagsAt72);
        writer.WriteUInt64(WideAt80);
        writer.WriteUtf16(TextAt88);
        writer.WriteUInt32(BooleanAt112);
        WriteValues(writer, ValuesAt116);
        writer.WriteUInt16(ShortAt128);
    }

    SessionDescriptor SessionDescriptor::Read(PacketReader& reader)
    {
        SessionDescriptor value;
        value.ShortAt0 = reader.ReadUInt16();
        value.ValueAt4 = reader.ReadUInt32();
        value.TextAt8 = reader.ReadUtf16();
        for (auto& item : value.ShortsAt32) { item = reader.ReadUInt16(); }
        value.BooleanAt38 = reader.ReadUInt32();
        value.TextAt40 = reader.ReadUtf16();
        value.SessionType = reader.ReadUInt32();
        value.Configuration = ReadConfiguration(reader);
        value.ValueAt68 = reader.ReadUInt32();
        ReadValues(reader, value.FlagsAt72);
        value.WideAt80 = reader.ReadUInt64();
        value.TextAt88 = reader.ReadUtf16();
        value.BooleanAt112 = reader.ReadUInt32();
        ReadValues(reader, value.ValuesAt116);
        value.ShortAt128 = reader.ReadUInt16();
        return value;
    }
}
