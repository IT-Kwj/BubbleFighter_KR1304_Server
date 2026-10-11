#pragma once

#include "TcpPacketCodec.h"

#include <string>

namespace BubbleFighter::Server::Protocol
{
    // E028: sent by BFCLIENT02 0x009A4E90 after the name confirmation dialog.
    struct CreateUserInfoRequest
    {
        static constexpr std::uint16_t MessageId = 0x0020;

        std::u16string CharacterName;
        // E030: scene CharacterSpec chrType / colorIndex, in this wire order.
        std::uint32_t CharacterType = 0;
        std::uint32_t ColorIndex = 0;
        std::u16string UnknownString;
        std::uint32_t UnknownValue = 0;

        TcpPacket Encode() const;
        static CreateUserInfoRequest Decode(const TcpPacket& packet);
        bool operator==(const CreateUserInfoRequest&) const = default;
    };
}
