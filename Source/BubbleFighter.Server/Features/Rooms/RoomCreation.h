#pragma once

#include "RoomRegistry.h"
#include "RoomRoster.h"
#include "Features/Characters/CharacterStore.h"
#include "Protocol/CreateRoomRequest.h"
#include "Protocol/CreateRoomResponse.h"
#include "Protocol/GotoRoomNotification.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace BubbleFighter::Server::Features::Rooms
{
    // Local server policy. None of these values were recovered from the
    // original server; an empty or zero policy rejects every request.
    struct RoomCreationPolicy
    {
        // Configuration.ShortAt4 values (0x00856047: dialog +0x720) that this
        // server is prepared to host. The dialog offers the hard-coded table
        // built at 0x005C6580; large-room modes are always refused.
        std::vector<std::uint16_t> PlayableModes;
        // Configuration.ValuesAt20[0] from 0x005C8AC0: chkOption[0] -> 1
        // (hammer battle), else [2] -> 3 (speed pop), else [5] -> 4 (map
        // view), else 0. Labels: Ui/Stages/Lobby/DlgCreateRoom_add_survival.ui.
        std::vector<std::uint32_t> RoomKinds;
        std::size_t MaxTitleCodeUnits = 0;
        std::size_t MaxPasswordCodeUnits = 0;
    };

    // Trusted state of an authenticated character already in the plaza; the
    // same member/equipment snapshot the plaza S117 was built from.
    struct RoomCreationInput
    {
        std::uint64_t AuthenticatedAccountId = 0;
        Characters::CharacterRecord Character;
        Protocol::SessionMember Member;
        Protocol::PartySupplementaryRecord Equipment;
        // The creator's connection; required (RoomRegistry.h).
        RoomPacketSink Sink;
    };

    struct RoomCreationOutcome
    {
        Protocol::CreateRoomResult Result = Protocol::CreateRoomResult::Failed;
        // Refusals: the S102 to return to the requester. On success this is
        // empty: S102 and the room S117 went to the sink when the room was
        // published, ahead of any member's broadcast.
        std::vector<Protocol::TcpPacket> Responses;
        // Present only on success; the session keeps it while inside the room.
        RoomLease Room;
    };

    class RoomCreation
    {
    public:
        // 0x009D9250: a type-2 context bounds slot access with 16 entries
        // unless 0x01150EF0 selects the 36-slot variant.
        static constexpr std::uint32_t OrdinaryRoomSlots = 16;
        // Lobby dialog 0x00855AE0 only ORs these bits into Configuration.ValueAt0:
        // 0x1 chkOption[1] (round battle) or mode 19, 0x20 chkOption[3] (random
        // teams), 0x80 chkOption[7] (champions cup rule), 0x800 channel type 80
        // (0x00896870). chkOption[6] (organize contest) is the contest byte.
        static constexpr std::uint32_t OrdinaryOptionFlags = 0x0001 | 0x0020 | 0x0080 | 0x0800;
        // wgtPvp (0x01134B50): the dialog's default entry (0x005C58F4) and
        // enabled in every channel type (0x010E57D0 / 0x010E5870).
        static constexpr std::uint16_t StandardPvpMode = 1;
        // 0x005C8686 caps edtRoomTitle at 20 code units (UiEdit MaxLength,
        // +0x4F0 per 0x00FCA7EC), overriding the resource value 15.
        static constexpr std::size_t DialogTitleCodeUnits = 20;
        // DlgCreateRoom_add_survival.ui: edtPassword MaxLength="6".
        static constexpr std::size_t DialogPasswordCodeUnits = 6;
        // Local policy: the creator becomes the host in the first slot.
        static constexpr std::uint32_t HostSlot = 0;

        // Malformed requests (values the client writer cannot produce) and
        // invalid trusted input throw without touching the registry. Policy
        // refusals return only S102. Success commits the room after both
        // packets were fully encoded; any exception rolls the claim back.
        // The room starts waiting (descriptor ValueAt4 = 2, RoomCreation R20).
        static RoomCreationOutcome Create(RoomRegistry& registry, const RoomCreationPolicy& policy,
            const Protocol::CreateRoomRequest& request, RoomCreationInput input);

        // 0x01150EF0: modes that may switch the room context to 36 slots.
        static bool IsLargeRoomMode(std::uint16_t mode) noexcept;
    };
}
