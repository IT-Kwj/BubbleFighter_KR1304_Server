#include "WeaponPresetChangeResponse.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket WeaponPresetChangeResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt8(PresetIndex);
        writer.WriteUInt32(Result);
        return {MessageId, writer.TakeBytes()};
    }

    WeaponPresetChangeResponse WeaponPresetChangeResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid weapon preset change response."); }
        PacketReader reader(packet.Payload);
        WeaponPresetChangeResponse result{reader.ReadUInt8(), reader.ReadUInt32()};
        reader.RequireEnd();
        return result;
    }
}
