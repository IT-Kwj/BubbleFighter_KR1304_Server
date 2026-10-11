#include "CenterListNotification.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CenterListNotification::Encode() const
    {
        if (Gates.size() > MaxEntries)
        {
            throw std::invalid_argument("Center list exceeds local entry limit.");
        }
        PacketWriter writer;
        writer.WriteUInt32(static_cast<std::uint32_t>(Gates.size()));
        for (const auto& gate : Gates)
        {
            writer.WriteUInt32(gate.UnknownValueAt4);
            writer.WriteUInt32(gate.UnknownValueAt8);
            writer.WriteUInt32(gate.UnknownValueAt12);
            for (const auto byte : gate.Ipv4Address) { writer.WriteUInt8(byte); }
            writer.WriteUInt16(gate.Port);
        }
        return {MessageId, writer.TakeBytes()};
    }

    CenterListNotification CenterListNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected center list message id.");
        }
        PacketReader reader(packet.Payload);
        const auto count = reader.ReadUInt32();
        // GateInfo's x86 object is 24 bytes; its wire representation is 18.
        if (count > MaxEntries || count > reader.RemainingBytes() / 18)
        {
            throw std::invalid_argument("Invalid center list entry count.");
        }
        CenterListNotification message;
        message.Gates.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            CenterGateInfo gate;
            gate.UnknownValueAt4 = reader.ReadUInt32();
            gate.UnknownValueAt8 = reader.ReadUInt32();
            gate.UnknownValueAt12 = reader.ReadUInt32();
            for (auto& byte : gate.Ipv4Address) { byte = reader.ReadUInt8(); }
            gate.Port = reader.ReadUInt16();
            message.Gates.push_back(gate);
        }
        reader.RequireEnd();
        return message;
    }
}
