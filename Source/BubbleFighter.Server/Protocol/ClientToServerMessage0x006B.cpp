#include "ClientToServerMessage0x006B.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x006B::Encode() const
    {
        constexpr std::size_t fixedSize = 13 * 4 + 1 + 3 * 4 + 4;
        std::size_t size = fixedSize;
        for (const auto* text : {&Directory, &MapName, &MapKey})
        {
            if (text->size() > PacketWriter::MaxStringCodeUnits ||
                text->size() > (MaxPayloadBytes - size) / 2)
            { throw std::length_error("C107 exceeds local string or payload limit."); }
            size += text->size() * 2;
        }
        PacketWriter writer;
        for (const auto value : Rules) { writer.WriteUInt32(value); }
        writer.WriteUInt8(RuleFlags);
        writer.WriteUtf16(Directory);
        writer.WriteUtf16(MapName);
        writer.WriteUtf16(MapKey);
        writer.WriteUInt32(Difficulty);
        return {MessageId, writer.TakeBytes()};
    }

    ClientToServerMessage0x006B ClientToServerMessage0x006B::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid C107 message."); }
        if (packet.Payload.size() > MaxPayloadBytes) { throw std::length_error("C107 exceeds local payload limit."); }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x006B result;
        for (auto& value : result.Rules) { value = reader.ReadUInt32(); }
        result.RuleFlags = reader.ReadUInt8();
        result.Directory = reader.ReadUtf16();
        result.MapName = reader.ReadUtf16();
        result.MapKey = reader.ReadUtf16();
        result.Difficulty = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
