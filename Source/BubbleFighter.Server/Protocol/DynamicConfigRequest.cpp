#include "DynamicConfigRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket DynamicConfigRequest::Encode() const
    {
        std::uint32_t mask = 0;
        for (std::size_t index = 0; index < Checksums.size(); ++index)
        {
            if (Checksums[index]) { mask |= std::uint32_t{1} << index; }
        }
        PacketWriter writer;
        writer.WriteUInt32(mask);
        for (const auto& checksum : Checksums) { if (checksum) { writer.WriteUInt32(*checksum); } }
        return {MessageId, writer.TakeBytes()};
    }

    DynamicConfigRequest DynamicConfigRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected dynamic configuration request id."); }
        PacketReader reader(packet.Payload);
        const auto mask = reader.ReadUInt32();
        DynamicConfigRequest request;
        for (std::size_t index = 0; index < request.Checksums.size(); ++index)
        {
            if (mask & (std::uint32_t{1} << index)) { request.Checksums[index] = reader.ReadUInt32(); }
        }
        reader.RequireEnd();
        return request;
    }
}
