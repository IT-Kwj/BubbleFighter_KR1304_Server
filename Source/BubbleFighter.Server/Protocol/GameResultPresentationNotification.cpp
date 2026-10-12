#include "GameResultPresentationNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameResultPresentationNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt8(Flags);
        return {MessageId, writer.TakeBytes()};
    }
    GameResultPresentationNotification GameResultPresentationNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid result presentation message."); }
        PacketReader reader(packet.Payload);
        GameResultPresentationNotification result{reader.ReadUInt8()};
        reader.RequireEnd();
        return result;
    }
}
