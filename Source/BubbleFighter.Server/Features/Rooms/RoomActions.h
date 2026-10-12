#pragma once

#include "RoomRegistry.h"
#include "Features/Characters/CharacterStore.h"
#include "Protocol/GameLoadNotification.h"
#include "Protocol/JoinRoomRequest.h"
#include "Protocol/JoinRoomResponse.h"
#include "Protocol/RoomListRequest.h"
#include "Protocol/RoomListResponse.h"
#include "Protocol/RoomReadyRequest.h"
#include "Protocol/StartGameRequest.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace BubbleFighter::Server::Features::Rooms
{
    // Local server policy, not recovered from the original server.
    struct RoomPlayPolicy
    {
        // Members (host included) required before a start is accepted.
        // User-authorized local solo play; other occupants must still be ready.
        std::size_t MinimumPlayersToStart = 1;
        // Bound on each forwarded C503 map text; the client loads MapResource.
        std::size_t MaxMapTextCodeUnits = 260;
    };

    // Same trusted plaza snapshot a room creator supplies (RoomCreationInput).
    struct RoomJoinInput
    {
        std::uint64_t AuthenticatedAccountId = 0;
        Characters::CharacterRecord Character;
        Protocol::SessionMember Member;
        Protocol::PartySupplementaryRecord Equipment;
        RoomPacketSink Sink;
    };

    struct RoomJoinOutcome
    {
        Protocol::JoinRoomResult Result = Protocol::JoinRoomResult::NoMatchingRoom;
        // Refusals: the S103 to return to the requester. Empty when joined:
        // S103 and the room S117 went to the sink inside the registry.
        std::vector<Protocol::TcpPacket> Responses;
        RoomLease Room;
    };

    class RoomActions
    {
    public:
        // C401 -> S101. Throws for page sizes the client writer cannot send.
        static Protocol::RoomListResponse List(const RoomRegistry& registry, const Protocol::RoomListRequest& request);
        // C403 -> S103 (+ S117 and the members' S503 when joined).
        static RoomJoinOutcome Join(RoomRegistry& registry, const Protocol::JoinRoomRequest& request, RoomJoinInput input);
        // C505 -> S503 to every member. Returns false while the room plays.
        static bool SetReady(RoomLease& room, const Protocol::RoomReadyRequest& request);
        // C503 -> S510 to every member when accepted. Refusals send nothing:
        // no refusal message for this request is known, and the start button
        // retries after its 2000 ms cooldown (0x00970590).
        static RoomStartStatus Start(RoomLease& room, const RoomPlayPolicy& policy, const Protocol::StartGameRequest& request);
        // The S510 sent for an accepted C503 (RoomCreation R22).
        static Protocol::GameLoadNotification CreateGameLoad(const RoomPlayPolicy& policy,
            const Protocol::StartGameRequest& request);
    };
}
