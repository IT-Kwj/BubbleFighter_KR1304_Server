#include "GameScoreNotification.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameScoreNotification::Encode() const
    {
        if (EntriesAt19188.size() > MaxEntries) { throw std::length_error("Game score map exceeds local limit."); }
        std::size_t size = Scores.EncodedSize() + 17 + EntriesAt19188.size() * 12 + (ExtensionTag == 0x3A ? 12 : 0);
        if (size > MaxPayloadBytes) { throw std::length_error("Game score notification too large."); }
        for (const auto& text : TextsAt19200)
        {
            if ((FlagsAt19272 & 4) == 0)
            {
                if (!text.empty()) { throw std::invalid_argument("Game score texts require flag bit 2."); }
            }
            else
            {
                if (text.size() > PacketWriter::MaxStringCodeUnits || size > MaxPayloadBytes - 4 ||
                    text.size() > (MaxPayloadBytes - size - 4) / 2)
                { throw std::length_error("Game score extension text too large."); }
                size += 4 + text.size() * 2;
            }
        }
        if (ExtensionTag != 0x3A && ValuesAt19276 != std::array<std::uint32_t, 3>{})
        { throw std::invalid_argument("Game score extension values require tag 0x3A."); }
        PacketWriter writer;
        Scores.Write(writer);
        writer.WriteUInt32(ValueAt19180); writer.WriteUInt32(ElapsedMilliseconds);
        writer.WriteUInt32(static_cast<std::uint32_t>(EntriesAt19188.size()));
        for (const auto& [key, values] : EntriesAt19188)
        { writer.WriteUInt32(key); writer.WriteUInt32(values[0]); writer.WriteUInt32(values[1]); }
        writer.WriteUInt8(ByteAt19196); writer.WriteUInt8(ByteAt19197); writer.WriteUInt16(FlagsAt19272);
        if ((FlagsAt19272 & 4) != 0) { for (const auto& text : TextsAt19200) { writer.WriteUtf16(text); } }
        writer.WriteUInt8(ExtensionTag);
        if (ExtensionTag == 0x3A) { for (const auto value : ValuesAt19276) { writer.WriteUInt32(value); } }
        return {MessageId, writer.TakeBytes()};
    }

    GameScoreNotification GameScoreNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game score message."); }
        if (packet.Payload.size() > MaxPayloadBytes) { throw std::length_error("Game score notification too large."); }
        PacketReader reader(packet.Payload);
        GameScoreNotification result;
        result.Scores = GameScoreRecord::Read(reader);
        result.ValueAt19180 = reader.ReadUInt32(); result.ElapsedMilliseconds = reader.ReadUInt32();
        const auto count = reader.ReadUInt32();
        if (count > MaxEntries || count > reader.RemainingBytes() / 12)
        { throw std::length_error("Game score map invalid or truncated."); }
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto key = reader.ReadUInt32();
            const std::array<std::uint32_t, 2> values{reader.ReadUInt32(), reader.ReadUInt32()};
            // Native map keeps the first duplicate; this implementation rejects ambiguity.
            if (!result.EntriesAt19188.emplace(key, values).second)
            { throw std::invalid_argument("Duplicate game score map key."); }
        }
        result.ByteAt19196 = reader.ReadUInt8(); result.ByteAt19197 = reader.ReadUInt8();
        result.FlagsAt19272 = reader.ReadUInt16();
        if ((result.FlagsAt19272 & 4) != 0) { for (auto& text : result.TextsAt19200) { text = reader.ReadUtf16(); } }
        result.ExtensionTag = reader.ReadUInt8();
        if (result.ExtensionTag == 0x3A) { for (auto& value : result.ValuesAt19276) { value = reader.ReadUInt32(); } }
        reader.RequireEnd();
        return result;
    }
}
