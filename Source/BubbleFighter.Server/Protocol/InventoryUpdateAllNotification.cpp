#include "InventoryUpdateAllNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    InventoryUpdateAllNotification InventoryUpdateAllNotification::FromRecords(std::span<const InventoryItemRecord> records)
    {
        return {ZlibStoredStream::Encode(InventoryItemRecord::EncodeList(records))};
    }

    TcpPacket InventoryUpdateAllNotification::Encode() const
    {
        if (CompressedRecords.empty() || CompressedRecords.size() > MaxCompressedBytes)
        { throw std::invalid_argument("Invalid compressed inventory size."); }
        PacketWriter writer;
        writer.WriteUInt32(static_cast<std::uint32_t>(CompressedRecords.size()));
        for (const auto value : CompressedRecords) { writer.WriteUInt8(value); }
        return {MessageId, writer.TakeBytes()};
    }

    InventoryUpdateAllNotification InventoryUpdateAllNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        { throw std::invalid_argument("Unexpected inventory notification id."); }
        PacketReader reader(packet.Payload);
        const auto count = reader.ReadUInt32();
        if (count == 0 || count > MaxCompressedBytes || count != reader.RemainingBytes())
        { throw std::invalid_argument("Invalid compressed inventory length."); }
        InventoryUpdateAllNotification result;
        result.CompressedRecords.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        { result.CompressedRecords.push_back(reader.ReadUInt8()); }
        reader.RequireEnd();
        return result;
    }
}
