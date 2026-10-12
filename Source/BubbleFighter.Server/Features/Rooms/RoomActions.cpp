#include "RoomActions.h"
#include "RoomMapCatalog.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace BubbleFighter::Server::Features::Rooms
{
    namespace
    {
        bool IsForwardableMapText(const std::u16string& text, std::size_t limit)
        {
            return !text.empty() && text.size() <= limit &&
                std::none_of(text.begin(), text.end(), [](char16_t unit) { return unit < 0x20; });
        }

        std::vector<Protocol::TcpPacket> Refuse(Protocol::JoinRoomResult result)
        {
            return {Protocol::JoinRoomResponse{static_cast<std::uint32_t>(result), 0}.Encode()};
        }
    }

    Protocol::RoomListResponse RoomActions::List(const RoomRegistry& registry, const Protocol::RoomListRequest& request)
    {
        // 0x00858E70 is the only writer and always sends 10.
        if (request.PageSize != Protocol::RoomListRequest::ClientPageSize)
        { throw std::invalid_argument("Room-list page size cannot come from the client writer."); }
        auto page = registry.List(request.Page, request.PageSize);
        Protocol::RoomListResponse response;
        response.Page = request.Page;
        response.HasNextPage = page.HasNextPage ? 1U : 0U;
        response.Rooms = std::move(page.Rooms);
        return response;
    }

    RoomJoinOutcome RoomActions::Join(RoomRegistry& registry, const Protocol::JoinRoomRequest& request, RoomJoinInput input)
    {
        if (input.AuthenticatedAccountId == 0 || input.Character.AccountId != input.AuthenticatedAccountId ||
            input.Character.CharacterId == 0 || input.Character.CharacterId > std::numeric_limits<std::uint32_t>::max() ||
            input.Equipment.Key != input.Character.CharacterId || !input.Sink)
        { throw std::invalid_argument("Room joiner does not belong to the authenticated account."); }
        RoomJoinOutcome outcome;
        // The client sign-extends a positive ShortAt0; anything else names no
        // room this server published.
        if (request.RoomId == 0 || request.RoomId > RoomRegistry::MaxRoomId)
        {
            outcome.Responses = Refuse(outcome.Result);
            return outcome;
        }
        RoomOccupant joiner;
        joiner.AccountId = input.AuthenticatedAccountId;
        joiner.Character = std::move(input.Character);
        joiner.Member = std::move(input.Member);
        joiner.Equipment = std::move(input.Equipment);
        // A fresh member is not ready (RoomCreation R19 bytes +96..+98).
        joiner.Member.BooleansAt96 = {};
        // Code 0 shows nothing (0x00853E90); the room S117 then selects stage 4.
        std::vector<Protocol::TcpPacket> prefix{Protocol::JoinRoomResponse{}.Encode()};
        auto joined = registry.Join(static_cast<std::uint16_t>(request.RoomId), request.Password, std::move(joiner),
            std::move(input.Sink), std::move(prefix));
        switch (joined.Status)
        {
        case RoomJoinStatus::Joined:
            outcome.Result = Protocol::JoinRoomResult::Joined;
            outcome.Room = std::move(joined.Room);
            return outcome;
        case RoomJoinStatus::Playing: outcome.Result = Protocol::JoinRoomResult::GameStarted; break;
        case RoomJoinStatus::WrongPassword: outcome.Result = Protocol::JoinRoomResult::WrongPassword; break;
        case RoomJoinStatus::Full: outcome.Result = Protocol::JoinRoomResult::RoomFull; break;
        case RoomJoinStatus::NoSuchRoom: outcome.Result = Protocol::JoinRoomResult::NoMatchingRoom; break;
        }
        outcome.Responses = Refuse(outcome.Result);
        return outcome;
    }

    bool RoomActions::SetReady(RoomLease& room, const Protocol::RoomReadyRequest& request)
    {
        // C505 order is {state 1, state 3, state 2}; the member bytes are
        // +96 state 1, +97 state 2, +98 state 3 (0x009DA951). Normalized to 1.
        const auto flag = [](std::uint32_t value) { return value != 0 ? 1U : 0U; };
        return room.SetMemberState({flag(request.Ready), flag(request.ValueAt8), flag(request.ValueAt4)});
    }

    Protocol::GameLoadNotification RoomActions::CreateGameLoad(const RoomPlayPolicy& policy,
        const Protocol::StartGameRequest& request)
    {
        // 0x009E5E70 starts nothing unless the key and the third text are
        // non-empty, and loads MapResource. Forwarded texts are bounded.
        if (!IsForwardableMapText(request.MapKey, policy.MaxMapTextCodeUnits) ||
            !IsForwardableMapText(request.MapResource, policy.MaxMapTextCodeUnits) ||
            !IsForwardableMapText(request.MapAttribute, policy.MaxMapTextCodeUnits))
        { throw std::invalid_argument("Start-game map selection cannot be forwarded."); }
        Protocol::GameLoadNotification start;
        // E089's native binding: wire texts 0/24/48 -> context 208/232/184.
        // Start resolves the accepted resource to a concrete catalog key first.
        // 9E4D10/7603B0 use +184 for MapMeta lookup; forwarding Random here
        // would use +232 (the display name), yielding a null resource.
        start.TextsAt0 = {request.MapResource, request.MapAttribute, request.MapKey};
        start.ValueAt72 = static_cast<std::uint32_t>(RoomState::Loading);
        // Remaining fields and maps: local zero/empty defaults (R22).
        return start;
    }

    RoomStartStatus RoomActions::Start(RoomLease& room, const RoomPlayPolicy& policy, const Protocol::StartGameRequest& request)
    {
        if (policy.MinimumPlayersToStart == 0) { throw std::invalid_argument("Room start policy allows an empty room."); }
        // 0x010E49C0 yields an empty attribute when the resource lacks it; the
        // client would then ignore S510, so such a start is refused instead.
        if (!IsForwardableMapText(request.MapKey, policy.MaxMapTextCodeUnits) ||
            !IsForwardableMapText(request.MapResource, policy.MaxMapTextCodeUnits) ||
            !IsForwardableMapText(request.MapAttribute, policy.MaxMapTextCodeUnits))
        { return RoomStartStatus::UnsupportedMap; }
        const auto* map = RoomMapCatalog::Match(request);
        if (!map) { return RoomStartStatus::UnsupportedMap; }
        auto resolved = request;
        resolved.MapKey = map->Key;
        const auto packet = CreateGameLoad(policy, resolved).Encode();
        return room.Start(packet, policy.MinimumPlayersToStart, map->Rules.Values[7], map->Key);
    }
}
