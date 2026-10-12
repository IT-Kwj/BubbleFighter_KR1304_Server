#pragma once
#include "SessionDescriptor.h"
#include "TcpPacketCodec.h"
namespace BubbleFighter::Server::Protocol
{
    // 9D9930 case 501 -> 113BD20, the shared S117 descriptor reader.
    struct SceneDescriptorNotification
    {
        static constexpr std::uint16_t MessageId = 501;
        SessionDescriptor Descriptor;
        TcpPacket Encode() const;
        static SceneDescriptorNotification Decode(const TcpPacket& packet);
        bool operator==(const SceneDescriptorNotification&) const = default;
    };
}
