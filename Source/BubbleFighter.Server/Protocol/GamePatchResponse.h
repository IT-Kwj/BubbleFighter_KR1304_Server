#pragma once

#include "TcpPacketCodec.h"

#include <string>

namespace BubbleFighter::Server::Protocol
{
    struct GamePatchResponse
    {
        static constexpr std::uint16_t MessageId = 0x0002;
        // E011: zero continues to stage 10; nonzero selects the error branch.
        std::uint32_t Result = 0;
        std::u16string Message;

        TcpPacket Encode() const;
        static GamePatchResponse Decode(const TcpPacket& packet);
    };
}
