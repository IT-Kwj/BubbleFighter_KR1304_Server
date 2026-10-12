#include "GameReportRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameReportRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(RandomSeed);
        for (const auto value : ValuesAt4) { writer.WriteUInt32(value); }
        return {MessageId, writer.TakeBytes()};
    }
    GameReportRequest GameReportRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game report message."); }
        PacketReader reader(packet.Payload);
        GameReportRequest result;
        result.RandomSeed = reader.ReadUInt32();
        for (auto& value : result.ValuesAt4) { value = reader.ReadUInt32(); }
        reader.RequireEnd();
        return result;
    }
}
