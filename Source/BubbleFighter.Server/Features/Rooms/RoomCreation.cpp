#include "RoomCreation.h"

#include "RoomRoster.h"
#include "Protocol/PacketWriter.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Rooms
{
    namespace
    {
        template <typename Value>
        bool Contains(const std::vector<Value>& values, Value value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        void ValidatePolicy(const RoomCreationPolicy& policy)
        {
            if (std::any_of(policy.PlayableModes.begin(), policy.PlayableModes.end(), RoomCreation::IsLargeRoomMode) ||
                policy.MaxTitleCodeUnits > Protocol::PacketWriter::MaxStringCodeUnits ||
                policy.MaxPasswordCodeUnits > Protocol::PacketWriter::MaxStringCodeUnits)
            { throw std::invalid_argument("Room creation policy exceeds the supported room shapes."); }
        }

        void ValidateRequestShape(const Protocol::CreateRoomRequest& request)
        {
            // 0x009622A3 clears ShortAt0 and no creation path assigns
            // SessionType after 0x0113B7B0 sets it to 0. The server owns both.
            if (request.Settings.ShortAt0 != 0 || request.Settings.SessionType != 0)
            { throw std::invalid_argument("Create-room request carries a server-owned room identity."); }
            // 0x00962272..0x00962284: the flag is exactly "password not empty".
            if (request.Settings.BooleanAt38 != (request.Password.empty() ? 0U : 1U))
            { throw std::invalid_argument("Create-room password flag does not match the password."); }
        }

        RoomCreationOutcome Refuse(Protocol::CreateRoomResult result)
        {
            RoomCreationOutcome outcome;
            outcome.Result = result;
            outcome.Responses.push_back(Protocol::CreateRoomResponse{0, static_cast<std::uint32_t>(result), 0}.Encode());
            return outcome;
        }
    }

    bool RoomCreation::IsLargeRoomMode(std::uint16_t mode) noexcept
    {
        // 0x01150EF0 returns true for 32, and for these when 0x01150F80 is
        // false. That runtime condition is unknown, so all are treated as large.
        switch (mode)
        {
        case 0x17: case 0x19: case 0x1E: case 0x20: case 0x2A: case 0x2C: case 0x2D: return true;
        default: return false;
        }
    }

    RoomCreationOutcome RoomCreation::Create(RoomRegistry& registry, const RoomCreationPolicy& policy,
        const Protocol::CreateRoomRequest& request, RoomCreationInput input)
    {
        using Protocol::CreateRoomResult;
        ValidatePolicy(policy);
        ValidateRequestShape(request);
        const auto& character = input.Character;
        if (input.AuthenticatedAccountId == 0 || character.AccountId != input.AuthenticatedAccountId ||
            character.CharacterId == 0 || character.CharacterId > std::numeric_limits<std::uint32_t>::max() ||
            input.Equipment.Key != character.CharacterId || !input.Sink)
        { throw std::invalid_argument("Room host does not belong to the authenticated account."); }

        const auto& settings = request.Settings;
        const auto& configuration = settings.Configuration;
        if (IsLargeRoomMode(configuration.ShortAt4) || !Contains(policy.PlayableModes, configuration.ShortAt4))
        { return Refuse(CreateRoomResult::ModeUnavailable); }
        // Unsupported creation variants: contest rooms (0x00856146 branch on
        // BooleanAt8), other dialogs' flag bits or trailing value, and local
        // title/password limits. They are refused rather than reinterpreted.
        if (request.TrailingValue || configuration.BooleanAt8 != 0 ||
            configuration.ValueAt0 != 0 ||
            !Contains(policy.RoomKinds, configuration.ValuesAt20[0]) ||
            settings.TextAt8.empty() || settings.TextAt8.size() > policy.MaxTitleCodeUnits ||
            request.Password.size() > policy.MaxPasswordCodeUnits)
        { return Refuse(CreateRoomResult::Failed); }

        // Local policy: one room per account/character; a repeated request
        // while a room or claim exists is refused without changing it.
        auto reserved = registry.Reserve(input.AuthenticatedAccountId, character.CharacterId);
        if (reserved.Status != RoomReserveStatus::Reserved) { return Refuse(CreateRoomResult::Failed); }
        auto& reservation = *reserved.Reservation;

        // The host's chosen settings are kept; RoomRoster::Describe assigns
        // the server-owned room number (C403 identity, 0x00855310), state,
        // counts and S117 type whenever a descriptor is sent.
        RoomRecord record;
        record.RoomId = reservation.RoomId();
        record.SlotCount = OrdinaryRoomSlots;
        record.HostSlot = HostSlot;
        record.State = RoomState::Waiting;
        record.Settings = settings;
        record.Password = request.Password;
        RoomOccupant host;
        host.Slot = HostSlot;
        host.AccountId = input.AuthenticatedAccountId;
        host.Character = std::move(input.Character);
        host.Member = std::move(input.Member);
        // 96DD20 accepts active teams 1/2; 36 is reserved for spectators.
        host.Member.SignedByteAt4 = 1;
        host.Member.BooleansAt96 = {};
        host.Equipment = std::move(input.Equipment);
        record.Occupants.push_back(std::move(host));

        // 0x00856F14 marks this client as creator before S117 builds stage 4.
        // Only the host entry is sent; 0x009D9250 default-constructs the other
        // slots and copies members by index below the 16-slot bound.
        std::vector<Protocol::TcpPacket> packets;
        packets.push_back(Protocol::CreateRoomResponse{}.Encode());
        packets.push_back(RoomRoster::Snapshot(record, HostSlot).Encode());

        RoomCreationOutcome outcome;
        outcome.Result = CreateRoomResult::Created;
        outcome.Room = std::move(reservation).Commit(std::move(record), std::move(input.Sink), std::move(packets));
        return outcome;
    }
}
