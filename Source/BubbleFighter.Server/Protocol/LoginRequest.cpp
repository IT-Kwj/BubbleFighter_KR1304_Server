#include "LoginRequest.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket LoginRequest::Encode() const
    {
        if (LocalIpv4Addresses.size() > MaxLocalIpv4Addresses)
        {
            throw std::length_error("Login address count exceeds supported limit.");
        }
        PacketWriter writer;
        writer.WriteUtf16(NexonPassport);
        writer.WriteUtf16(HardwareId);
        writer.WriteUtf16(UnknownString);
        writer.WriteUInt32(static_cast<std::uint32_t>(LocalIpv4Addresses.size()));
        for (const auto& address : LocalIpv4Addresses)
        {
            for (const auto octet : address)
            {
                writer.WriteUInt8(octet);
            }
        }
        writer.WriteUInt32(ResourceValue);
        writer.WriteUInt32(UnknownValue);
        writer.WriteUtf16(PlatformString);
        return {MessageId, writer.TakeBytes()};
    }

    LoginRequest LoginRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected login request message id.");
        }
        PacketReader reader(packet.Payload);
        LoginRequest message;
        message.NexonPassport = reader.ReadUtf16();
        message.HardwareId = reader.ReadUtf16();
        message.UnknownString = reader.ReadUtf16();
        const auto count = reader.ReadUInt32();
        if (count > MaxLocalIpv4Addresses)
        {
            throw std::length_error("Login address count exceeds supported limit.");
        }
        message.LocalIpv4Addresses.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            std::array<std::uint8_t, 4> address{};
            for (auto& octet : address)
            {
                octet = reader.ReadUInt8();
            }
            message.LocalIpv4Addresses.push_back(address);
        }
        message.ResourceValue = reader.ReadUInt32();
        message.UnknownValue = reader.ReadUInt32();
        message.PlatformString = reader.ReadUtf16();
        reader.RequireEnd();
        return message;
    }
}
