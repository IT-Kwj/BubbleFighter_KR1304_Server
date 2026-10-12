#pragma once

#include "GotoRoomNotification.h"
#include "TcpPacketCodec.h"

#include <cstdint>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R16: S503, room dispatcher 0x009D9930 case 0x1F7 ->
    // 0x009DA720. Replaces one slot's member (a previously empty slot is a
    // join), merges both equipment maps entry by entry (0x0081D060 by slot,
    // 0x0081D010 by player) and installs the team object (0x009EF840).
    struct RoomSlotUpdateNotification
    {
        static constexpr std::uint16_t MessageId = 0x01F7;

        std::uint32_t Slot = 0;
        SessionMember Member;
        std::vector<PartySupplementaryRecord> RecordsAt564;
        std::vector<PartySupplementaryRecord> RecordsAt588;
        SessionTeam Team;

        TcpPacket Encode() const;
        static RoomSlotUpdateNotification Decode(const TcpPacket& packet);
        bool operator==(const RoomSlotUpdateNotification&) const = default;
    };
}
