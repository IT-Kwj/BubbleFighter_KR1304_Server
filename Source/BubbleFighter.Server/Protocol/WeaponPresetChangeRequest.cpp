#include "WeaponPresetChangeRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket WeaponPresetChangeRequest::Encode() const
    {
        if (PresetIndex > MaxPresetIndex) { throw std::invalid_argument("Invalid weapon preset index."); }
        PacketWriter writer;
        writer.WriteUInt32(ObjectId);
        writer.WriteUInt8(PresetIndex);
        return {MessageId, writer.TakeBytes()};
    }

    WeaponPresetChangeRequest WeaponPresetChangeRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid weapon preset change request."); }
        PacketReader reader(packet.Payload);
        WeaponPresetChangeRequest result{reader.ReadUInt32(), reader.ReadUInt8()};
        reader.RequireEnd();
        if (result.PresetIndex > MaxPresetIndex) { throw std::invalid_argument("Invalid weapon preset index."); }
        return result;
    }
}
