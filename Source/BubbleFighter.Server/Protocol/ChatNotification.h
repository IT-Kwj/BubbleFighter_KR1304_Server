#pragma once
#include "TcpPacketCodec.h"
#include <string>
namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 0x10C4A50, dispatched by S509 at 0x9D9930.
    // The last UTF-16 string and boolean are absent from the old client format.
    struct ChatNotification
    {
        static constexpr std::uint16_t MessageId = 509;
        std::int16_t Slot = -3;
        std::u16string Sender;
        std::uint32_t Channel = 0;
        std::uint32_t ValueAt32 = 0;
        std::u16string Expression; // "e" enables ordinary actor chat in 0x9EAC70.
        std::u16string Text;
        std::uint32_t TextColor = 0xFFFFFFFF;
        std::uint32_t NameColor = 0xFFFFFFFF;
        bool FlagAt92 = false;
        std::u16string StringAt96; // Optional validated category/motion, e.g. emotion/love.
        bool FlagAt120 = false;
        TcpPacket Encode() const;
        static ChatNotification Decode(const TcpPacket& packet);
        bool operator==(const ChatNotification&) const = default;
    };
}
