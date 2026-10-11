#include "LoginResponse.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket LoginResponse::Encode() const
    {
        if ((Result == 0) != Success.has_value() ||
            (National.Result == 0) != National.Details.has_value() ||
            (TailMarker == ExtensionMarker) != TailValue.has_value())
        {
            throw std::invalid_argument("Login response fields do not match their discriminators.");
        }
        PacketWriter writer;
        writer.WriteUInt32(Result);
        if (Success)
        {
            writer.WriteUtf16(Success->AccountIdentifier);
        }
        writer.WriteUInt32(IdentityValue);
        if (Success)
        {
            writer.WriteUtf16(Success->CharacterName);
            writer.WriteUInt32(Success->CharacterType);
            writer.WriteUInt32(Success->UnknownValue);
            writer.WriteUInt32(Success->ChannelCategory);
            writer.WriteUInt32(Success->UnknownConnectionValue);
            writer.WriteUInt64(Success->ServerTimeBits);
            writer.WriteUInt32(Success->GameMasterValue);
        }
        writer.WriteUInt32(National.Result);
        if (National.Details)
        {
            writer.WriteUInt8(National.Details->ByteValue);
            writer.WriteUtf16(National.Details->FirstString);
            writer.WriteUtf16(National.Details->SecondString);
            writer.WriteUInt32(National.Details->BooleanValue);
        }
        writer.WriteUInt8(TailMarker);
        if (TailValue)
        {
            writer.WriteUInt32(*TailValue);
        }
        return {MessageId, writer.TakeBytes()};
    }

    LoginResponse LoginResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected login response message id.");
        }
        PacketReader reader(packet.Payload);
        LoginResponse message;
        message.Result = reader.ReadUInt32();
        if (message.Result == 0)
        {
            message.Success.emplace();
            message.Success->AccountIdentifier = reader.ReadUtf16();
        }
        message.IdentityValue = reader.ReadUInt32();
        if (message.Success)
        {
            message.Success->CharacterName = reader.ReadUtf16();
            message.Success->CharacterType = reader.ReadUInt32();
            message.Success->UnknownValue = reader.ReadUInt32();
            message.Success->ChannelCategory = reader.ReadUInt32();
            message.Success->UnknownConnectionValue = reader.ReadUInt32();
            message.Success->ServerTimeBits = reader.ReadUInt64();
            message.Success->GameMasterValue = reader.ReadUInt32();
        }
        message.National.Result = reader.ReadUInt32();
        if (message.National.Result == 0)
        {
            message.National.Details.emplace();
            auto& details = *message.National.Details;
            details.ByteValue = reader.ReadUInt8();
            details.FirstString = reader.ReadUtf16();
            details.SecondString = reader.ReadUtf16();
            details.BooleanValue = reader.ReadUInt32();
        }
        message.TailMarker = reader.ReadUInt8();
        if (message.TailMarker == ExtensionMarker)
        {
            message.TailValue = reader.ReadUInt32();
        }
        reader.RequireEnd();
        return message;
    }
}
