#include "GotoLobbyNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GotoLobbyNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(PartyMoveNotice);
        return {MessageId, writer.TakeBytes()};
    }

    GotoLobbyNotification GotoLobbyNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected goto-lobby id."); }
        PacketReader reader(packet.Payload);
        GotoLobbyNotification notification{reader.ReadUInt32()};
        reader.RequireEnd();
        return notification;
    }
}
