#include "RoomRoster.h"

#include "Features/Scenes/SceneRoster.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace BubbleFighter::Server::Features::Rooms
{
    Protocol::SessionDescriptor RoomRoster::Describe(const RoomRecord& room)
    {
        if (room.Occupants.empty() || room.SlotCount == 0 || room.SlotCount > Scenes::SceneRoster::MaxSlots)
        { throw std::invalid_argument("Room record has no members or an unsupported slot count."); }
        auto descriptor = room.Settings;
        descriptor.ShortAt0 = room.RoomId;
        descriptor.ValueAt4 = static_cast<std::uint32_t>(room.State);
        descriptor.SessionType = 2;
        // Local choice: members over room capacity, nothing subtracted. The
        // creation request leaves all three at zero (RoomCreation R15).
        descriptor.ShortsAt32 = {static_cast<std::uint16_t>(room.Occupants.size()), 0,
            static_cast<std::uint16_t>(room.SlotCount)};
        return descriptor;
    }

    Protocol::GotoRoomNotification RoomRoster::Snapshot(const RoomRecord& room, std::uint32_t localSlot)
    {
        const auto* local = room.FindSlot(localSlot);
        if (!local) { throw std::invalid_argument("Room snapshot for an empty slot."); }
        Protocol::GotoRoomNotification envelope;
        envelope.Descriptor = Describe(room);
        // 0x009EBF60 compares this host slot with the local slot (+728).
        envelope.ValueAt544 = room.HostSlot;
        envelope.Team = room.Team;
        // Members are positional: the vector reaches the highest occupied
        // slot and empty entries stay default (0x009D9250 copies by index).
        std::vector<Scenes::SceneSlot> slots(room.Occupants.back().Slot + 1);
        for (const auto& occupant : room.Occupants)
        {
            auto& slot = slots.at(occupant.Slot);
            slot.State = occupant.Member;
            slot.Character = occupant.Character;
            slot.Equipment = occupant.Equipment;
        }
        return Scenes::SceneRoster::ComposeRoom(std::move(envelope), slots, localSlot, local->AccountId);
    }

    Protocol::RoomSlotUpdateNotification RoomRoster::SlotUpdate(const RoomRecord& room, std::uint32_t slot)
    {
        auto snapshot = Snapshot(room, slot);
        Protocol::RoomSlotUpdateNotification update;
        update.Slot = slot;
        update.Member = std::move(snapshot.Members.at(slot));
        // SceneRoster keys +564 by slot and +588 by player identity.
        const auto bySlot = std::find_if(snapshot.RecordsAt564.begin(), snapshot.RecordsAt564.end(),
            [&](const auto& record) { return record.Key == slot; });
        const auto byPlayer = std::find_if(snapshot.RecordsAt588.begin(), snapshot.RecordsAt588.end(),
            [&](const auto& record) { return record.Key == update.Member.ValueAt8; });
        if (bySlot == snapshot.RecordsAt564.end() || byPlayer == snapshot.RecordsAt588.end())
        { throw std::logic_error("Room snapshot lacks the slot's equipment records."); }
        update.RecordsAt564.push_back(std::move(*bySlot));
        update.RecordsAt588.push_back(std::move(*byPlayer));
        update.Team = room.Team;
        return update;
    }

    Protocol::RoomMemberLeftNotification RoomRoster::MemberLeft(const RoomRecord& room, std::uint32_t slot)
    {
        if (slot >= room.SlotCount || room.FindSlot(slot))
        { throw std::invalid_argument("Member-left notification for an occupied or invalid slot."); }
        Protocol::RoomMemberLeftNotification message;
        message.Slot = slot;
        // Default member: BooleansAt0[0] (occupied) is 0 and +204 is absent.
        message.Team = room.Team;
        return message;
    }
}
