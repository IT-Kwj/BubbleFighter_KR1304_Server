#include "WeaponPresetUpdateNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket WeaponPresetUpdateNotification::Encode() const
    {
        if (Records.size() > MaxCollectionEntries || Bytes.size() > MaxCollectionEntries)
        { throw std::invalid_argument("Weapon preset collection exceeds local limit."); }
        PacketWriter writer;
        writer.WriteUInt8(ByteAt0);
        writer.WriteUInt32(PlayerId);
        writer.WriteUInt32(static_cast<std::uint32_t>(Records.size()));
        for (const auto& record : Records)
        {
            for (const auto value : record) { writer.WriteUInt32(value); }
        }
        writer.WriteUInt32(static_cast<std::uint32_t>(Bytes.size()));
        for (const auto value : Bytes) { writer.WriteUInt8(value); }
        return {MessageId, writer.TakeBytes()};
    }

    WeaponPresetUpdateNotification WeaponPresetUpdateNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        { throw std::invalid_argument("Unexpected weapon preset notification id."); }
        PacketReader reader(packet.Payload);
        WeaponPresetUpdateNotification result;
        result.ByteAt0 = reader.ReadUInt8();
        result.PlayerId = reader.ReadUInt32();
        const auto count = reader.ReadUInt32();
        if (count > MaxCollectionEntries || count > reader.RemainingBytes() / 12)
        { throw std::invalid_argument("Invalid weapon preset count."); }
        result.Records.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            result.Records.push_back({reader.ReadUInt32(), reader.ReadUInt32(), reader.ReadUInt32()});
        }
        const auto byteCount = reader.ReadUInt32();
        if (byteCount > MaxCollectionEntries || byteCount > reader.RemainingBytes())
        { throw std::invalid_argument("Invalid weapon preset byte count."); }
        result.Bytes.reserve(byteCount);
        for (std::uint32_t index = 0; index < byteCount; ++index)
        { result.Bytes.push_back(reader.ReadUInt8()); }
        reader.RequireEnd();
        return result;
    }
}
