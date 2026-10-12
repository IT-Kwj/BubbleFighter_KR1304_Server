#pragma once
#include "TcpPacketCodec.h"
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 0x4E9070: channel, text, optional character expression.
    struct ChatRequest
    {
        static constexpr std::uint16_t MessageId = 502;
        std::uint32_t Channel = 0;
        std::u16string Text;
        std::u16string Expression;
        TcpPacket Encode() const;
        static ChatRequest Decode(const TcpPacket& packet);
        bool operator==(const ChatRequest&) const = default;
    };
}
