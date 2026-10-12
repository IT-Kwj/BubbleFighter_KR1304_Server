#include "CashInventoryUpdateNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CashInventoryUpdateNotification::Encode() const
    {
        if (Slots.size() > MaxSlots) { throw std::invalid_argument("Too many cash inventory slots."); }
        std::size_t bytes = 4;
        for (const auto& slot : Slots)
        {
            if (slot.TextAt24.size() > PacketWriter::MaxStringCodeUnits ||
                25 + 2 * slot.TextAt24.size() > MaxPayloadBytes - bytes)
            { throw std::invalid_argument("Cash inventory payload exceeds local limits."); }
            bytes += 25 + 2 * slot.TextAt24.size();
        }
        PacketWriter writer;
        writer.WriteUInt32(static_cast<std::uint32_t>(Slots.size()));
        for (const auto& slot : Slots)
        {
            writer.WriteUInt32(slot.SlotId);
            writer.WriteUInt32(slot.ValueAt8);
            writer.WriteUInt16(slot.Quantity);
            writer.WriteUInt16(slot.DurationDays);
            writer.WriteUInt32(slot.StockId);
            writer.WriteUInt32(slot.ItemId);
            writer.WriteUtf16(slot.TextAt24);
            writer.WriteUInt8(slot.ByteAt48);
        }
        return {MessageId, writer.TakeBytes()};
    }

    CashInventoryUpdateNotification CashInventoryUpdateNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId || packet.Payload.size() > MaxPayloadBytes)
        { throw std::invalid_argument("Invalid cash inventory message or size."); }
        PacketReader reader(packet.Payload);
        const auto count = reader.ReadUInt32();
        if (count > MaxSlots || count > reader.RemainingBytes() / 25)
        { throw std::invalid_argument("Invalid cash inventory slot count."); }
        CashInventoryUpdateNotification result;
        result.Slots.reserve(count);
        for (std::uint32_t i = 0; i != count; ++i)
        {
            CashInventorySlotRecord slot;
            slot.SlotId = reader.ReadUInt32();
            slot.ValueAt8 = reader.ReadUInt32();
            slot.Quantity = reader.ReadUInt16();
            slot.DurationDays = reader.ReadUInt16();
            slot.StockId = reader.ReadUInt32();
            slot.ItemId = reader.ReadUInt32();
            slot.TextAt24 = reader.ReadUtf16();
            slot.ByteAt48 = reader.ReadUInt8();
            result.Slots.push_back(std::move(slot));
        }
        reader.RequireEnd();
        return result;
    }
}
