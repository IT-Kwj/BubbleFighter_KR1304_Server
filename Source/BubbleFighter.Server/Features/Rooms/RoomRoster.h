#pragma once

#include "RoomRegistry.h"
#include "Protocol/GotoRoomNotification.h"
#include "Protocol/RoomMemberLeftNotification.h"
#include "Protocol/RoomSlotUpdateNotification.h"

#include <cstdint>

namespace BubbleFighter::Server::Features::Rooms
{
    // Packets derived from a room record. Pure: nothing here sends or stores.
    class RoomRoster
    {
    public:
        // The descriptor of a room, shared by its S117 and its S101 entry:
        // server identity, type 2, state and the counts the list renders as
        // "(ShortsAt32[0] - ShortsAt32[1]) / ShortsAt32[2]" (0x0063CF90).
        static Protocol::SessionDescriptor Describe(const RoomRecord& room);
        // Room S117 for the member in localSlot (0x009D9250 bounds the slots).
        static Protocol::GotoRoomNotification Snapshot(const RoomRecord& room, std::uint32_t localSlot);
        // S503 for an occupied slot: the member and its two equipment records
        // exactly as the room S117 carries them.
        static Protocol::RoomSlotUpdateNotification SlotUpdate(const RoomRecord& room, std::uint32_t slot);
        // S511 emptying a slot.
        static Protocol::RoomMemberLeftNotification MemberLeft(const RoomRecord& room, std::uint32_t slot);
    };
}
