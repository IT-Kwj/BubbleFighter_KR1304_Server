#include "RoomListResponse.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        // Smallest SessionDescriptor encoding: fixed fields plus empty strings
        // and collections; guards the count before reserving.
        constexpr std::size_t MinimumDescriptorBytes = 28;
    }

    TcpPacket RoomListResponse::Encode() const
    {
        if (Rooms.size() > MaxRooms) { throw std::length_error("Room list exceeds local limit."); }
        PacketWriter writer;
        writer.WriteUInt32(Page);
        writer.WriteUInt32(HasNextPage);
        writer.WriteUInt32(ValueAt8);
        writer.WriteUInt32(static_cast<std::uint32_t>(Rooms.size()));
        for (const auto& room : Rooms) { room.Write(writer); }
        return {MessageId, writer.TakeBytes()};
    }

    RoomListResponse RoomListResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected room-list response id."); }
        PacketReader reader(packet.Payload);
        RoomListResponse response;
        response.Page = reader.ReadUInt32();
        response.HasNextPage = reader.ReadUInt32();
        response.ValueAt8 = reader.ReadUInt32();
        const auto count = reader.ReadUInt32();
        if (count > MaxRooms || count > reader.RemainingBytes() / MinimumDescriptorBytes)
        { throw std::length_error("Room list count exceeds the payload or local limit."); }
        response.Rooms.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) { response.Rooms.push_back(SessionDescriptor::Read(reader)); }
        reader.RequireEnd();
        return response;
    }
}
