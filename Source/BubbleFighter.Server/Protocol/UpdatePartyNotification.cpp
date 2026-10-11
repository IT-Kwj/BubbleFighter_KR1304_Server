#include "UpdatePartyNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        void Charge(std::size_t count, std::size_t& budget)
        {
            if (count > UpdatePartyNotification::MaxCollectionEntries || count > budget)
            { throw std::invalid_argument("Party collection exceeds local resource limit."); }
            budget -= count;
        }
        std::uint32_t ReadCount(PacketReader& reader, std::size_t minimumBytes, std::size_t& budget)
        {
            const auto count = reader.ReadUInt32();
            Charge(count, budget);
            if (count > reader.RemainingBytes() / minimumBytes)
            { throw std::invalid_argument("Truncated party collection."); }
            return count;
        }
        void WriteCount(PacketWriter& writer, std::size_t count, std::size_t& budget)
        {
            Charge(count, budget);
            writer.WriteUInt32(static_cast<std::uint32_t>(count));
        }
    }

    void PartySupplementaryRecord::Write(PacketWriter& writer, std::size_t& budget) const
    {
        writer.WriteUInt32(Key);
        writer.WriteUInt32(Value);
        for (const auto& group : Groups)
        {
            WriteCount(writer, group.size(), budget);
            for (const auto value : group) { writer.WriteUInt32(value); }
        }
        WriteCount(writer, Bytes.size(), budget);
        for (const auto value : Bytes) { writer.WriteUInt8(value); }
        WriteCount(writer, Details.size(), budget);
        for (const auto& detail : Details)
        {
            for (const auto value : detail.Values) { writer.WriteUInt32(value); }
            WriteCount(writer, detail.Entries.size(), budget);
            for (const auto value : detail.Entries) { writer.WriteUInt32(value); }
            writer.WriteUInt32(detail.FinalValue);
            writer.WriteUtf16(detail.Text);
        }
    }

    PartySupplementaryRecord PartySupplementaryRecord::Read(PacketReader& reader, std::size_t& budget)
    {
        PartySupplementaryRecord record;
        record.Key = reader.ReadUInt32();
        record.Value = reader.ReadUInt32();
        for (auto& group : record.Groups)
        {
            const auto size = ReadCount(reader, 4, budget);
            group.reserve(size);
            for (std::uint32_t j = 0; j < size; ++j) { group.push_back(reader.ReadUInt32()); }
        }
        const auto bytes = ReadCount(reader, 1, budget);
        record.Bytes.reserve(bytes);
        for (std::uint32_t j = 0; j < bytes; ++j) { record.Bytes.push_back(reader.ReadUInt8()); }
        const auto details = ReadCount(reader, 20, budget);
        record.Details.reserve(details);
        for (std::uint32_t j = 0; j < details; ++j)
        {
            auto& detail = record.Details.emplace_back();
            for (auto& value : detail.Values) { value = reader.ReadUInt32(); }
            const auto entries = ReadCount(reader, 4, budget);
            detail.Entries.reserve(entries);
            for (std::uint32_t k = 0; k < entries; ++k) { detail.Entries.push_back(reader.ReadUInt32()); }
            detail.FinalValue = reader.ReadUInt32();
            detail.Text = reader.ReadUtf16();
        }
        return record;
    }

    TcpPacket UpdatePartyNotification::Encode() const
    {
        PacketWriter writer;
        std::size_t budget = MaxTotalEntries;
        writer.WriteUInt32(State);
        WriteCount(writer, Members.size(), budget);
        for (const auto& member : Members)
        {
            writer.WriteUInt32(member.IdentityValue);
            writer.WriteUtf16(member.CharacterName);
            for (const auto value : member.ValuesAt32) { writer.WriteUInt32(value); }
        }
        WriteCount(writer, SupplementaryRecords.size(), budget);
        for (const auto& record : SupplementaryRecords)
        {
            record.Write(writer, budget);
        }
        return {MessageId, writer.TakeBytes()};
    }

    UpdatePartyNotification UpdatePartyNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected party message id."); }
        PacketReader reader(packet.Payload);
        std::size_t budget = MaxTotalEntries;
        UpdatePartyNotification result;
        result.State = reader.ReadUInt32();
        auto count = ReadCount(reader, 24, budget);
        result.Members.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            auto& member = result.Members.emplace_back();
            member.IdentityValue = reader.ReadUInt32();
            member.CharacterName = reader.ReadUtf16();
            for (auto& value : member.ValuesAt32) { value = reader.ReadUInt32(); }
        }
        count = ReadCount(reader, 28, budget);
        result.SupplementaryRecords.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            auto& record = result.SupplementaryRecords.emplace_back();
            record = PartySupplementaryRecord::Read(reader, budget);
        }
        reader.RequireEnd();
        return result;
    }
}
