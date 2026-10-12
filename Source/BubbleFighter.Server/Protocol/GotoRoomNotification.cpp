#include "GotoRoomNotification.h"

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

        void Charge(std::size_t count, std::size_t& budget)
        {
            if (count > GotoRoomNotification::MaxCollectionEntries || count > budget)
            { throw std::length_error("Session snapshot exceeds local collection limit."); }
            budget -= count;
        }

        template <typename Entry, typename WriteEntry>
        void WriteEntries(PacketWriter& writer, const std::vector<Entry>& entries,
            std::size_t& budget, WriteEntry writeEntry)
        {
            Charge(entries.size(), budget);
            writer.WriteUInt32(static_cast<std::uint32_t>(entries.size()));
            for (const auto& entry : entries) { writeEntry(entry); }
        }

        template <typename Entry, typename ReadEntry>
        std::vector<Entry> ReadEntries(PacketReader& reader, std::size_t& budget,
            std::size_t minimumBytes, ReadEntry readEntry)
        {
            const auto count = reader.ReadUInt32();
            Charge(count, budget);
            if (count > reader.RemainingBytes() / minimumBytes)
            { throw std::runtime_error("Truncated session snapshot collection."); }
            std::vector<Entry> entries;
            entries.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index) { entries.push_back(readEntry()); }
            return entries;
        }

        void WriteVector(PacketWriter& writer, const std::vector<std::uint32_t>& values, std::size_t& budget)
        {
            WriteEntries(writer, values, budget, [&](const auto value) { writer.WriteUInt32(value); });
        }

        std::vector<std::uint32_t> ReadVector(PacketReader& reader, std::size_t& budget)
        {
            return ReadEntries<std::uint32_t>(reader, budget, 4, [&] { return reader.ReadUInt32(); });
        }

        template <typename Value, typename WriteValue>
        void WriteOptional(PacketWriter& writer, std::uint8_t marker,
            const std::optional<Value>& value, WriteValue writeValue)
        {
            if ((marker == GotoRoomNotification::PresentMarker) != value.has_value())
            { throw std::invalid_argument("Session snapshot optional value does not match its marker."); }
            writer.WriteUInt8(marker);
            if (value) { writeValue(*value); }
        }

        void WriteMember(PacketWriter& writer, const SessionMember& value, std::size_t& budget)
        {
            WriteValues(writer, value.BooleansAt0);
            writer.WriteUInt8(value.ByteAt3);
            writer.WriteUInt8(value.SignedByteAt4);
            writer.WriteUInt32(value.ValueAt8);
            writer.WriteUtf16(value.TextAt12);
            for (const auto item : value.SignedShortsAt36) { writer.WriteUInt16(item); }
            WriteValues(writer, value.BooleansAt96);
            WriteValues(writer, value.ValuesAt44);
            WriteVector(writer, value.ValuesAt52, budget);
            WriteValues(writer, value.ValuesAt64);
            writer.WriteUInt32(value.ValueAt88);
            writer.WriteUInt32(value.BooleanAt94);
            writer.WriteUInt32(value.ValueAt100);
            writer.WriteUInt32(value.BooleanAt104);
            writer.WriteUInt32(value.ValueAt108);
            writer.WriteUInt32(value.BooleanAt112);
            writer.WriteUtf16(value.TextAt116);
            writer.WriteUInt32(value.ValueAt140);
            writer.WriteUtf16(value.TextAt144);
            WriteValues(writer, value.BooleansAt168);
            WriteValues(writer, value.ValuesAt172);
            writer.WriteUInt8(value.ByteAt184);
            writer.WriteUInt32(value.BooleanAt185);
            WriteValues(writer, value.ValuesAt188);
            WriteOptional(writer, value.MarkerAt204, value.OptionalAt204, [&](const auto& fields)
            {
                WriteValues(writer, fields.ValuesAt204);
                WriteOptional(writer, fields.MarkerAt212, fields.OptionalAt212,
                    [&](const auto& entries) { WriteValues(writer, entries); });
            });
            writer.WriteUInt32(value.BooleanAt232);
            WriteValues(writer, value.Social.Values);
            writer.WriteUInt32(value.Social.BooleanValue);
            writer.WriteUInt64(value.Social.WideValue);
            WriteValues(writer, value.Social.BooleanValues);
        }

        SessionMember ReadMember(PacketReader& reader, std::size_t& budget)
        {
            SessionMember value;
            ReadValues(reader, value.BooleansAt0);
            value.ByteAt3 = reader.ReadUInt8();
            value.SignedByteAt4 = reader.ReadUInt8();
            value.ValueAt8 = reader.ReadUInt32();
            value.TextAt12 = reader.ReadUtf16();
            for (auto& item : value.SignedShortsAt36) { item = reader.ReadUInt16(); }
            ReadValues(reader, value.BooleansAt96);
            ReadValues(reader, value.ValuesAt44);
            value.ValuesAt52 = ReadVector(reader, budget);
            ReadValues(reader, value.ValuesAt64);
            value.ValueAt88 = reader.ReadUInt32();
            value.BooleanAt94 = reader.ReadUInt32();
            value.ValueAt100 = reader.ReadUInt32();
            value.BooleanAt104 = reader.ReadUInt32();
            value.ValueAt108 = reader.ReadUInt32();
            value.BooleanAt112 = reader.ReadUInt32();
            value.TextAt116 = reader.ReadUtf16();
            value.ValueAt140 = reader.ReadUInt32();
            value.TextAt144 = reader.ReadUtf16();
            ReadValues(reader, value.BooleansAt168);
            ReadValues(reader, value.ValuesAt172);
            value.ByteAt184 = reader.ReadUInt8();
            value.BooleanAt185 = reader.ReadUInt32();
            ReadValues(reader, value.ValuesAt188);
            value.MarkerAt204 = reader.ReadUInt8();
            if (value.MarkerAt204 == GotoRoomNotification::PresentMarker)
            {
                auto& fields = value.OptionalAt204.emplace();
                ReadValues(reader, fields.ValuesAt204);
                fields.MarkerAt212 = reader.ReadUInt8();
                if (fields.MarkerAt212 == GotoRoomNotification::PresentMarker)
                { ReadValues(reader, fields.OptionalAt212.emplace()); }
            }
            value.BooleanAt232 = reader.ReadUInt32();
            ReadValues(reader, value.Social.Values);
            value.Social.BooleanValue = reader.ReadUInt32();
            value.Social.WideValue = reader.ReadUInt64();
            ReadValues(reader, value.Social.BooleanValues);
            return value;
        }

        void WriteFields(PacketWriter& writer, const SessionOptionalFields& value, std::size_t& budget)
        {
            writer.WriteUInt32(value.ValueAt0);
            writer.WriteUInt64(value.WideAt8);
            WriteEntries(writer, value.TextsAt16, budget, [&](const auto& text) { writer.WriteUtf16(text); });
            WriteOptional(writer, value.MarkerAt28, value.OptionalAt28, [&](const auto& group)
            {
                writer.WriteUInt32(group.Value);
                WriteVector(writer, group.Values, budget);
            });
            WriteValues(writer, value.ValuesAt48);
        }

        SessionOptionalFields ReadFields(PacketReader& reader, std::size_t& budget)
        {
            SessionOptionalFields value;
            value.ValueAt0 = reader.ReadUInt32();
            value.WideAt8 = reader.ReadUInt64();
            value.TextsAt16 = ReadEntries<std::u16string>(reader, budget, 4, [&] { return reader.ReadUtf16(); });
            value.MarkerAt28 = reader.ReadUInt8();
            if (value.MarkerAt28 == GotoRoomNotification::PresentMarker)
            {
                auto& group = value.OptionalAt28.emplace();
                group.Value = reader.ReadUInt32();
                group.Values = ReadVector(reader, budget);
            }
            ReadValues(reader, value.ValuesAt48);
            return value;
        }
    }

    void WriteSessionMember(PacketWriter& writer, const SessionMember& value, std::size_t& budget)
    { WriteMember(writer, value, budget); }
    SessionMember ReadSessionMember(PacketReader& reader, std::size_t& budget)
    { return ReadMember(reader, budget); }
    void WriteSessionRecords(PacketWriter& writer, const std::vector<PartySupplementaryRecord>& records, std::size_t& budget)
    { WriteEntries(writer, records, budget, [&](const auto& record) { record.Write(writer, budget); }); }
    std::vector<PartySupplementaryRecord> ReadSessionRecords(PacketReader& reader, std::size_t& budget)
    { return ReadEntries<PartySupplementaryRecord>(reader, budget, 28, [&] { return PartySupplementaryRecord::Read(reader, budget); }); }
    void WriteSessionTeam(PacketWriter& writer, const SessionTeam& team, std::size_t& budget)
    {
        WriteEntries(writer, team.Groups, budget, [&](const SessionTeamGroup& group)
        {
            writer.WriteUInt8(group.SignedKey);
            WriteVector(writer, group.Values, budget);
        });
        for (const auto& values : team.Values) { WriteVector(writer, values, budget); }
    }
    SessionTeam ReadSessionTeam(PacketReader& reader, std::size_t& budget)
    {
        SessionTeam team;
        team.Groups = ReadEntries<SessionTeamGroup>(reader, budget, 5, [&]
        {
            SessionTeamGroup group;
            group.SignedKey = reader.ReadUInt8();
            group.Values = ReadVector(reader, budget);
            return group;
        });
        for (auto& values : team.Values) { values = ReadVector(reader, budget); }
        return team;
    }

    TcpPacket GotoRoomNotification::Encode() const
    {
        PacketWriter writer;
        std::size_t budget = MaxTotalCollectionEntries;
        Descriptor.Write(writer);
        writer.WriteUInt32(ValueAt544);
        writer.WriteUInt32(ValueAt548);
        WriteEntries(writer, Members, budget, [&](const auto& member) { WriteMember(writer, member, budget); });
        WriteSessionRecords(writer, RecordsAt564, budget);
        WriteSessionRecords(writer, RecordsAt588, budget);
        WriteSessionTeam(writer, Team, budget);
        WriteOptional(writer, MarkerAt648, OptionalAt648,
            [&](const auto& fields) { WriteFields(writer, fields, budget); });
        writer.WriteUInt32(ValueAt728);
        writer.WriteUInt32(BooleanAt732);
        return {MessageId, writer.TakeBytes()};
    }

    GotoRoomNotification GotoRoomNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        { throw std::invalid_argument("Unexpected goto-room notification message id."); }
        PacketReader reader(packet.Payload);
        std::size_t budget = MaxTotalCollectionEntries;
        GotoRoomNotification message;
        message.Descriptor = SessionDescriptor::Read(reader);
        message.ValueAt544 = reader.ReadUInt32();
        message.ValueAt548 = reader.ReadUInt32();
        message.Members = ReadEntries<SessionMember>(reader, budget, 188, [&] { return ReadMember(reader, budget); });
        message.RecordsAt564 = ReadSessionRecords(reader, budget);
        message.RecordsAt588 = ReadSessionRecords(reader, budget);
        message.Team = ReadSessionTeam(reader, budget);
        message.MarkerAt648 = reader.ReadUInt8();
        if (message.MarkerAt648 == PresentMarker) { message.OptionalAt648 = ReadFields(reader, budget); }
        message.ValueAt728 = reader.ReadUInt32();
        message.BooleanAt732 = reader.ReadUInt32();
        reader.RequireEnd();
        return message;
    }
}
