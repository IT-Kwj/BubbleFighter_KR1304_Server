#include "GameExtendedShotNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameExtendedShotNotification::Encode() const
    {
        if (ValuesAt44.size() > MaxValues) { throw std::length_error("Shot extension exceeds payload policy."); }
        PacketWriter writer;
        Shot.Encode(writer);
        writer.WriteUInt32(ValueAt40);
        writer.WriteUInt32(static_cast<std::uint32_t>(ValuesAt44.size()));
        for (const auto value : ValuesAt44) { writer.WriteUInt32(value); }
        return {MessageId, writer.TakeBytes()};
    }

    GameExtendedShotNotification GameExtendedShotNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid extended shot notification."); }
        if (packet.Payload.size() > TcpPacketCodec::DefaultMaxPayloadSize)
        { throw std::length_error("Shot extension exceeds payload policy."); }
        PacketReader reader(packet.Payload);
        GameExtendedShotNotification result;
        result.Shot = CompressedShotRecord::Decode(reader);
        result.ValueAt40 = reader.ReadUInt32();
        const auto count = reader.ReadUInt32();
        // Check the complete count before allocating, including extra bytes.
        if (count > MaxValues || reader.RemainingBytes() % 4 != 0 || count != reader.RemainingBytes() / 4)
        { throw std::runtime_error("Invalid shot extension count."); }
        result.ValuesAt44.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) { result.ValuesAt44.push_back(reader.ReadUInt32()); }
        reader.RequireEnd();
        return result;
    }
}
