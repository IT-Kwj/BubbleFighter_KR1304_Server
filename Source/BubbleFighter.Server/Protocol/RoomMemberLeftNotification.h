#pragma once

#include "GotoRoomNotification.h"
#include "TcpPacketCodec.h"

#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R16: S511, room dispatcher 0x009D9930 case 0x1FF ->
    // 0x009DB1E0. Takes the previous occupant out of the slot (peer/UI
    // cleanup through 0x00921670 and 0x0090E8E0), stores the sent member in
    // its place and installs the team object. Departures send an empty member.
    struct RoomMemberLeftNotification
    {
        static constexpr std::uint16_t MessageId = 0x01FF;

        std::uint32_t Slot = 0;
        SessionMember Member;
        SessionTeam Team;

        TcpPacket Encode() const;
        static RoomMemberLeftNotification Decode(const TcpPacket& packet);
        bool operator==(const RoomMemberLeftNotification&) const = default;
    };
}
