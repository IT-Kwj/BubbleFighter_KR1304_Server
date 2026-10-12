#pragma once

#include "Features/Characters/CharacterStore.h"
#include "Protocol/GotoRoomNotification.h"
#include "Protocol/SessionDescriptor.h"
#include "Protocol/TcpPacketCodec.h"
#include "RoomGameSession.h"
#include "RoomSettlementQueue.h"
#include "Features/Chat/SceneChat.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace BubbleFighter::Server::Features::Rooms
{
    // Delivery to one connection. The registry calls it while holding its
    // lock, so every member observes a room's events in the same order and a
    // member's own result is never overtaken by a later broadcast. It must
    // only queue the packets: no blocking on a socket and no call back into
    // the registry. Exceptions are swallowed (the connection is closing).
    using RoomPacketSink = std::function<void(std::vector<Protocol::TcpPacket>)>;

    // Descriptor ValueAt4, which is room context +12 (RoomCreation R20):
    // 0x00970590 only sends a start request while it is 2, and 0x0094D7A0
    // names 4 "Play" (5 is "Gameover", not produced by this server).
    enum class RoomState : std::uint32_t { Waiting = 2, Loading = 3, Playing = 4 };

    struct RoomOccupant
    {
        std::uint32_t Slot = 0;
        std::uint64_t AccountId = 0;
        Characters::CharacterRecord Character;
        // Trusted plaza member state. BooleansAt96 carries the C505 room state
        // (+96 ready, +97 state 2, +98 state 3; RoomCreation R19).
        Protocol::SessionMember Member;
        Protocol::PartySupplementaryRecord Equipment;
        bool operator==(const RoomOccupant&) const = default;
    };

    // Server-side room state. Settings are the host's accepted creation
    // settings; RoomRoster derives every sent descriptor from them. The
    // password is compared on join and must never be logged.
    struct RoomRecord
    {
        std::uint16_t RoomId = 0;
        std::uint32_t SlotCount = 0;
        std::uint32_t HostSlot = 0;
        RoomState State = RoomState::Waiting;
        Protocol::SessionDescriptor Settings;
        std::u16string Password;
        // Sent unchanged in S117/S503/S511; no team assignment is implemented.
        Protocol::SessionTeam Team;
        // Ascending by slot; never empty for a published room.
        std::vector<RoomOccupant> Occupants;

        const RoomOccupant* FindSlot(std::uint32_t slot) const noexcept;
        bool operator==(const RoomRecord&) const = default;
    };

    class RoomRegistry;

    struct RoomInventoryChange
    {
        Characters::CharacterRecord Character;
        Protocol::PartySupplementaryRecord Equipment;
    };

    enum class RoomStartStatus { Started, NotHost, NotWaiting, NotEnoughPlayers, MembersNotReady, UnsupportedMap };

    // One member's place in a room. Releasing it (or destroying it, e.g. on
    // disconnect) leaves the room: the others receive S511 and, when the
    // host left, S502; the last member closes the room.
    class RoomLease
    {
    public:
        RoomLease() = default;
        RoomLease(RoomLease&& other) noexcept;
        RoomLease& operator=(RoomLease&& other) noexcept;
        RoomLease(const RoomLease&) = delete;
        RoomLease& operator=(const RoomLease&) = delete;
        ~RoomLease();

        std::uint16_t RoomId() const noexcept;
        explicit operator bool() const noexcept;
        void Release() noexcept;

        // Stores the member's C505 state and broadcasts S503. Returns false
        // (nothing sent) while the room is playing.
        bool SetMemberState(const std::array<std::uint32_t, 3>& state);
        // Host only, while waiting, with every other member ready and at least
        // minimumPlayers members: marks the room playing and broadcasts packet.
        RoomStartStatus Start(const Protocol::TcpPacket& packet, std::size_t minimumPlayers, std::uint32_t durationMilliseconds,
            std::u16string_view trustedMapKey = {});
        void ProcessGame(const Protocol::TcpPacket& packet);
        void Poll();
        void ReturnToWaiting();
        std::optional<Characters::CharacterRecord> Character() const;
        void Chat(const Features::Chat::SceneChat::Message& request);
        void RefreshCharacter(const Characters::CharacterRecord& character);
        bool ChangeInventory(const std::function<RoomInventoryChange()>& change);

    private:
        friend class RoomReservation;
        friend class RoomRegistry;
        RoomLease(std::shared_ptr<RoomRegistry> registry, std::uint16_t roomId, std::uint64_t accountId) noexcept;
        std::shared_ptr<RoomRegistry> registry_;
        std::uint16_t roomId_ = 0;
        std::uint64_t accountId_ = 0;
    };

    // Exclusive claim on an identity and an owner before anything is encoded.
    // Nothing is visible to Find/Count/List until Commit; destruction rolls back.
    class RoomReservation
    {
    public:
        RoomReservation(RoomReservation&& other) noexcept;
        RoomReservation& operator=(RoomReservation&& other) noexcept;
        RoomReservation(const RoomReservation&) = delete;
        RoomReservation& operator=(const RoomReservation&) = delete;
        ~RoomReservation();

        std::uint16_t RoomId() const noexcept;
        // The record must use the reserved identity with the reserved owner
        // as its only occupant, in the host slot. Publishing and delivering
        // hostPackets to hostSink happen under one lock. On failure the
        // reservation stays intact.
        RoomLease Commit(RoomRecord record, RoomPacketSink hostSink, std::vector<Protocol::TcpPacket> hostPackets) &&;

    private:
        friend class RoomRegistry;
        RoomReservation(std::shared_ptr<RoomRegistry> registry, std::uint16_t roomId,
            std::uint64_t accountId, std::uint64_t characterId) noexcept;
        void Abandon() noexcept;
        std::shared_ptr<RoomRegistry> registry_;
        std::uint16_t roomId_ = 0;
        std::uint64_t accountId_ = 0;
        std::uint64_t characterId_ = 0;
    };

    enum class RoomReserveStatus { Reserved, AlreadyInRoom, RegistryFull };

    struct RoomReserveResult
    {
        RoomReserveStatus Status = RoomReserveStatus::RegistryFull;
        std::optional<RoomReservation> Reservation;
    };

    enum class RoomJoinStatus { Joined, NoSuchRoom, Playing, WrongPassword, Full };

    struct RoomJoinResult
    {
        RoomJoinStatus Status = RoomJoinStatus::NoSuchRoom;
        // Present only when joined.
        RoomLease Room;
    };

    struct RoomListPage
    {
        std::vector<Protocol::SessionDescriptor> Rooms;
        bool HasNextPage = false;
    };

    // Process-wide room table shared by all connections; thread safe.
    class RoomRegistry : public std::enable_shared_from_this<RoomRegistry>
    {
    public:
        // 0x00855310 sign-extends descriptor ShortAt0 into the C403 join
        // request, so identities stay in the positive int16 range.
        static constexpr std::uint16_t MaxRoomId = 0x7FFF;

        // maxRooms is a local resource policy in [1, MaxRoomId].
        using GameClock = std::function<RoomGameSession::Time()>;
        using SeedFactory = std::function<std::uint32_t()>;
        static std::shared_ptr<RoomRegistry> Create(std::size_t maxRooms, GameClock clock = {}, SeedFactory seed = {},
            RoomMatchSettler settler = {}, LocalMatchRules rules = {});

        // An account or character is in at most one room or pending claim.
        RoomReserveResult Reserve(std::uint64_t accountId, std::uint64_t characterId);
        // Takes the lowest free slot. The joiner receives joinerPrefix, then
        // its room S117 (local slot = its slot); the others receive S503.
        // Throws when the joiner is already in a room or claim.
        RoomJoinResult Join(std::uint16_t roomId, const std::u16string& password, RoomOccupant joiner,
            RoomPacketSink sink, std::vector<Protocol::TcpPacket> joinerPrefix);
        // Rooms in identity order; pageSize > 0.
        RoomListPage List(std::size_t page, std::size_t pageSize) const;
        std::optional<RoomRecord> Find(std::uint16_t roomId) const;
        std::optional<std::uint16_t> FindByCharacter(std::uint64_t characterId) const;
        std::size_t Count() const;
        // Application-owned worker also calls this when there are no clients.
        void RetrySettlements();
        void RecoverSettlement(RoomMatchRequest request);
        std::size_t PendingSettlementCount() const;

    private:
        friend class RoomLease;
        friend class RoomReservation;
        struct Owner
        {
            std::uint64_t AccountId = 0;
            std::uint64_t CharacterId = 0;
        };
        struct Room
        {
            RoomRecord Record;
            std::map<std::uint32_t, RoomPacketSink> Sinks;
            std::optional<RoomGameSession> Game;
            std::optional<RoomGameSession> FinishedGame;
            std::array<std::uint8_t, 16> MatchId{};
            std::uint32_t DurationMilliseconds = 0;
            std::shared_ptr<RoomSettlementQueue::Job> PendingSettlement;
            bool SettlementComplete = false, SettlementDelayed = false;
            std::set<std::uint64_t> InventoryChanges;
            RoomGameSession::Time RetrySettlementAt{};
        };
        explicit RoomRegistry(std::size_t maxRooms, GameClock clock, SeedFactory seed,
            RoomMatchSettler settler, LocalMatchRules rules);
        void Publish(std::uint16_t roomId, std::uint64_t accountId, std::uint64_t characterId, RoomRecord record,
            RoomPacketSink hostSink, std::vector<Protocol::TcpPacket> hostPackets);
        void Abandon(std::uint16_t roomId) noexcept;
        void Leave(std::uint16_t roomId, std::uint64_t accountId) noexcept;
        bool SetMemberState(std::uint16_t roomId, std::uint64_t accountId, const std::array<std::uint32_t, 3>& state);
        RoomStartStatus Start(std::uint16_t roomId, std::uint64_t accountId, const Protocol::TcpPacket& packet,
            std::size_t minimumPlayers, std::uint32_t durationMilliseconds, std::u16string_view trustedMapKey);
        void ProcessGame(std::uint16_t roomId, std::uint64_t accountId, const Protocol::TcpPacket& packet);
        void Poll(std::uint16_t roomId, std::uint64_t accountId);
        void ReturnToWaiting(std::uint16_t roomId, std::uint64_t accountId);
        void Chat(std::uint16_t roomId, std::uint64_t accountId, const Features::Chat::SceneChat::Message& request);
        void RefreshCharacter(std::uint16_t roomId, std::uint64_t accountId, const Characters::CharacterRecord& character);
        bool ChangeInventory(std::uint16_t roomId, std::uint64_t accountId, const std::function<RoomInventoryChange()>& change);
        void PollLocked(Room& room, RoomGameSession::Time now);
        void SettleLocked(Room& room, RoomGameSession::Time now);
        static void ResetGameLocked(Room& room, bool returnNotified = false);
        static void DeliverGameLocked(const Room& room, const std::vector<RoomGameDelivery>& deliveries);
        bool IsUsedLocked(std::uint16_t roomId) const;
        RoomOccupant* FindMemberLocked(Room& room, std::uint64_t accountId);
        static void Deliver(const RoomPacketSink& sink, std::vector<Protocol::TcpPacket> packets) noexcept;
        static void Broadcast(const Room& room, const std::vector<Protocol::TcpPacket>& packets,
            std::optional<std::uint32_t> exceptSlot = std::nullopt) noexcept;

        mutable std::mutex mutex_;
        std::size_t maxRooms_ = 0;
        std::uint16_t nextRoomId_ = 1;
        GameClock clock_;
        SeedFactory seed_;
        std::shared_ptr<RoomSettlementQueue> settlements_;
        LocalMatchRules rules_;
        std::map<std::uint16_t, Owner> pending_;
        std::map<std::uint16_t, Room> rooms_;
        std::unordered_map<std::uint64_t, std::uint16_t> byAccount_;
        std::unordered_map<std::uint64_t, std::uint16_t> byCharacter_;
    };
}
