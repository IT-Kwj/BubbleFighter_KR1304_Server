#include "GameLoadNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <bit>
#include <stdexcept>
#include <type_traits>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        void Charge(std::size_t count, std::size_t& budget)
        {
            if (count > GameLoadNotification::MaxCollectionEntries || count > budget)
            { throw std::length_error("Game load collection exceeds local limit."); }
            budget -= count;
        }

        template <typename Key>
        void WriteEntries(PacketWriter& writer, const std::map<Key, std::array<std::uint32_t, 2>>& entries)
        {
            writer.WriteUInt32(static_cast<std::uint32_t>(entries.size()));
            for (const auto& [key, values] : entries)
            {
                if constexpr (std::is_same_v<Key, std::int8_t>) { writer.WriteUInt8(std::bit_cast<std::uint8_t>(key)); }
                else { writer.WriteUInt32(key); }
                writer.WriteUInt32(values[0]);
                writer.WriteUInt32(values[1]);
            }
        }

        template <typename Key>
        std::map<Key, std::array<std::uint32_t, 2>> ReadEntries(PacketReader& reader, std::size_t& budget)
        {
            const auto count = reader.ReadUInt32();
            Charge(count, budget);
            constexpr std::size_t entrySize = std::is_same_v<Key, std::int8_t> ? 9 : 12;
            if (count > reader.RemainingBytes() / entrySize)
            { throw std::runtime_error("Truncated game load collection."); }
            std::map<Key, std::array<std::uint32_t, 2>> entries;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                Key key;
                if constexpr (std::is_same_v<Key, std::int8_t>) { key = std::bit_cast<std::int8_t>(reader.ReadUInt8()); }
                else { key = reader.ReadUInt32(); }
                const std::array<std::uint32_t, 2> values{reader.ReadUInt32(), reader.ReadUInt32()};
                // Local strictness: original trees retain the first duplicate.
                if (!entries.emplace(key, values).second)
                { throw std::invalid_argument("Duplicate game load collection key."); }
            }
            return entries;
        }
    }

    TcpPacket GameLoadNotification::Encode() const
    {
        std::size_t budget = MaxTotalCollectionEntries;
        Charge(EntriesAt96.size(), budget);
        Charge(EntriesAt104.size(), budget);
        Charge(EntriesAt112.size(), budget);
        std::size_t size = 39 + EntriesAt96.size() * 9 + EntriesAt104.size() * 12 + EntriesAt112.size() * 9;
        for (const auto& text : TextsAt0)
        {
            if (text.size() > PacketWriter::MaxStringCodeUnits || text.size() > (MaxPayloadBytes - size) / 2)
            { throw std::length_error("Game load exceeds local string or payload limit."); }
            size += text.size() * 2;
        }
        PacketWriter writer;
        for (const auto& text : TextsAt0) { writer.WriteUtf16(text); }
        writer.WriteUInt32(ValueAt72);
        writer.WriteUInt32(ValueAt76);
        writer.WriteUInt8(SignedByteAt80);
        writer.WriteUInt8(FlagsAt84);
        writer.WriteUInt32(ValueAt88);
        writer.WriteUInt8(ByteAt92);
        WriteEntries(writer, EntriesAt96);
        WriteEntries(writer, EntriesAt104);
        WriteEntries(writer, EntriesAt112);
        return {MessageId, writer.TakeBytes()};
    }

    GameLoadNotification GameLoadNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game load message."); }
        if (packet.Payload.size() > MaxPayloadBytes) { throw std::length_error("Game load exceeds local payload limit."); }
        PacketReader reader(packet.Payload);
        GameLoadNotification result;
        for (auto& text : result.TextsAt0) { text = reader.ReadUtf16(); }
        result.ValueAt72 = reader.ReadUInt32();
        result.ValueAt76 = reader.ReadUInt32();
        result.SignedByteAt80 = reader.ReadUInt8();
        result.FlagsAt84 = reader.ReadUInt8();
        result.ValueAt88 = reader.ReadUInt32();
        result.ByteAt92 = reader.ReadUInt8();
        std::size_t budget = MaxTotalCollectionEntries;
        result.EntriesAt96 = ReadEntries<std::int8_t>(reader, budget);
        result.EntriesAt104 = ReadEntries<std::uint32_t>(reader, budget);
        result.EntriesAt112 = ReadEntries<std::int8_t>(reader, budget);
        reader.RequireEnd();
        return result;
    }
}
