#include "UserStatNotification.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket UserStatNotification::Encode() const
    {
        PacketWriter writer;
        for (const auto value : Values) { writer.WriteUInt32(value); }
        writer.WriteUtf16(Text);
        writer.WriteUInt16(ShortValue);
        Fighter.Write(writer);
        return {MessageId, writer.TakeBytes()};
    }

    UserStatNotification UserStatNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected user-stat notification message id.");
        }
        PacketReader reader(packet.Payload);
        UserStatNotification message;
        for (auto& value : message.Values) { value = reader.ReadUInt32(); }
        message.Text = reader.ReadUtf16();
        message.ShortValue = reader.ReadUInt16();
        message.Fighter = FighterInfo::Read(reader);
        reader.RequireEnd();
        return message;
    }
}
