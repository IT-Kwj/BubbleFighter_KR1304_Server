#include "DynamicConfigResponse.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket DynamicConfigResponse::Encode() const
    {
        std::uint32_t mask = 0;
        std::size_t total = 0;
        for (std::size_t index = 0; index < Records.size(); ++index)
        {
            if (!Records[index]) { continue; }
            const auto size = Records[index]->size();
            if (size == 0 || size > MaxCompressedBytes - total) { throw std::invalid_argument("Invalid compressed configuration size."); }
            total += size;
            mask |= std::uint32_t{1} << index;
        }
        PacketWriter writer;
        writer.WriteUInt32(mask);
        for (const auto& record : Records)
        {
            if (!record) { continue; }
            writer.WriteUInt32(static_cast<std::uint32_t>(record->size()));
            for (const auto byte : *record) { writer.WriteUInt8(byte); }
        }
        return {MessageId, writer.TakeBytes()};
    }

    DynamicConfigResponse DynamicConfigResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected dynamic configuration response id."); }
        PacketReader reader(packet.Payload);
        const auto mask = reader.ReadUInt32();
        DynamicConfigResponse response;
        std::size_t total = 0;
        for (std::size_t index = 0; index < response.Records.size(); ++index)
        {
            if (!(mask & (std::uint32_t{1} << index))) { continue; }
            const auto count = reader.ReadUInt32();
            if (count == 0 || count > MaxCompressedBytes - total || count > reader.RemainingBytes())
            { throw std::invalid_argument("Invalid compressed configuration length."); }
            total += count;
            auto& record = response.Records[index].emplace();
            record.reserve(count);
            for (std::uint32_t item = 0; item < count; ++item) { record.push_back(reader.ReadUInt8()); }
        }
        reader.RequireEnd();
        return response;
    }
}
