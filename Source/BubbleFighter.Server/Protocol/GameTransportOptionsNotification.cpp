#include "GameTransportOptionsNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameTransportOptionsNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(RelayEnabledValue);
        writer.WriteUInt32(UdpEnabledValue);
        writer.WriteUInt32(MovementDeltaEnabledValue);
        return {MessageId, writer.TakeBytes()};
    }

    GameTransportOptionsNotification GameTransportOptionsNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game transport options message."); }
        PacketReader reader(packet.Payload);
        GameTransportOptionsNotification result;
        result.RelayEnabledValue = reader.ReadUInt32();
        result.UdpEnabledValue = reader.ReadUInt32();
        result.MovementDeltaEnabledValue = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
