#include "RoomRegistry.h"
#include "Features/Chat/SceneChat.h"

#include "RoomRoster.h"
#include "Protocol/RoomHostChangedNotification.h"
#include "Protocol/GameTransportOptionsNotification.h"
#include "Protocol/GameReturnToRoomNotification.h"
#include "Features/Practice/PracticeSession.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Rooms
{
    const RoomOccupant* RoomRecord::FindSlot(std::uint32_t slot) const noexcept
    {
        const auto occupant = std::find_if(Occupants.begin(), Occupants.end(),
            [&](const RoomOccupant& entry) { return entry.Slot == slot; });
        return occupant == Occupants.end() ? nullptr : &*occupant;
    }

    RoomLease::RoomLease(std::shared_ptr<RoomRegistry> registry, std::uint16_t roomId, std::uint64_t accountId) noexcept :
        registry_(std::move(registry)), roomId_(roomId), accountId_(accountId) {}

    RoomLease::RoomLease(RoomLease&& other) noexcept :
        registry_(std::move(other.registry_)), roomId_(std::exchange(other.roomId_, std::uint16_t{})),
        accountId_(std::exchange(other.accountId_, std::uint64_t{})) {}

    RoomLease& RoomLease::operator=(RoomLease&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            registry_ = std::move(other.registry_);
            roomId_ = std::exchange(other.roomId_, std::uint16_t{});
            accountId_ = std::exchange(other.accountId_, std::uint64_t{});
        }
        return *this;
    }

    RoomLease::~RoomLease() { Release(); }

    std::uint16_t RoomLease::RoomId() const noexcept { return registry_ ? roomId_ : 0; }
    RoomLease::operator bool() const noexcept { return registry_ != nullptr; }

    void RoomLease::Release() noexcept
    {
        if (registry_) { registry_->Leave(roomId_, accountId_); }
        registry_.reset();
        roomId_ = 0;
        accountId_ = 0;
    }

    bool RoomLease::SetMemberState(const std::array<std::uint32_t, 3>& state)
    {
        if (!registry_) { throw std::logic_error("Room state change without a room."); }
        return registry_->SetMemberState(roomId_, accountId_, state);
    }

    RoomStartStatus RoomLease::Start(const Protocol::TcpPacket& packet, std::size_t minimumPlayers, std::uint32_t durationMilliseconds,
        std::u16string_view trustedMapKey)
    {
        if (!registry_) { throw std::logic_error("Game start without a room."); }
        return registry_->Start(roomId_, accountId_, packet, minimumPlayers, durationMilliseconds, trustedMapKey);
    }

    void RoomLease::ProcessGame(const Protocol::TcpPacket& packet)
    {
        if (!registry_) { throw std::logic_error("Game message outside a room."); }
        registry_->ProcessGame(roomId_, accountId_, packet);
    }
    void RoomLease::Poll() { if (registry_) { registry_->Poll(roomId_, accountId_); } }
    void RoomLease::Chat(const Features::Chat::SceneChat::Message& request)
    { if (registry_) { registry_->Chat(roomId_, accountId_, request); } }
    void RoomLease::RefreshCharacter(const Characters::CharacterRecord& character)
    { if (registry_) { registry_->RefreshCharacter(roomId_, accountId_, character); } }
    bool RoomLease::ChangeInventory(const std::function<RoomInventoryChange()>& change)
    { return registry_ && registry_->ChangeInventory(roomId_, accountId_, change); }
    bool RoomRegistry::ChangeInventory(std::uint16_t roomId, std::uint64_t accountId, const std::function<RoomInventoryChange()>& change)
    {
        {
            std::lock_guard lock(mutex_); auto& room = rooms_.at(roomId);
            if (!FindMemberLocked(room, accountId)) { throw std::logic_error("Inventory change by non-member."); }
            if (room.Record.State != RoomState::Waiting || !room.InventoryChanges.insert(accountId).second) { return false; }
        }
        // Reserve the waiting state, then release the registry lock for database
        // work. Start cannot bind stale equipment while the change is in flight.
        RoomInventoryChange result;
        try { result = change(); }
        catch (...)
        {
            std::lock_guard lock(mutex_); const auto found = rooms_.find(roomId);
            if (found != rooms_.end()) { found->second.InventoryChanges.erase(accountId); }
            throw;
        }
        std::lock_guard lock(mutex_); auto& room = rooms_.at(roomId); room.InventoryChanges.erase(accountId);
        auto* member = FindMemberLocked(room, accountId);
        if (!member || result.Character.AccountId != accountId || result.Character.CharacterId != member->Character.CharacterId ||
            result.Equipment.Key != result.Character.CharacterId || !result.Character.Statistics.IsValid())
        { throw std::invalid_argument("Room inventory identity mismatch."); }
        if (member->Character == result.Character && member->Equipment == result.Equipment) { return true; }
        const auto previous = *member;
        member->Character = std::move(result.Character); member->Equipment = std::move(result.Equipment);
        member->Member.BooleansAt96 = {}; // Changed loadout requires preparing again.
        Protocol::TcpPacket packet;
        try { packet = RoomRoster::SlotUpdate(room.Record, member->Slot).Encode(); }
        catch (...) { *member = previous; throw; }
        Broadcast(room, {packet}); return true;
    }
    void RoomRegistry::RefreshCharacter(std::uint16_t roomId, std::uint64_t accountId, const Characters::CharacterRecord& character)
    {
        std::lock_guard lock(mutex_); auto& room = rooms_.at(roomId); auto* member = FindMemberLocked(room, accountId);
        if (!member || character.AccountId != accountId || character.CharacterId != member->Character.CharacterId ||
            character.CharacterName != member->Character.CharacterName || character.CharacterType != member->Character.CharacterType ||
            character.ColorIndex != member->Character.ColorIndex || !character.Statistics.IsValid())
        { throw std::invalid_argument("Room commerce identity mismatch."); }
        // Shop transactions do not change the equipped loadout or room seat.
        // Keep the owned snapshot in sync so Poll cannot restore old balances.
        member->Character.Statistics = character.Statistics;
    }
    void RoomRegistry::Chat(std::uint16_t roomId, std::uint64_t accountId, const Features::Chat::SceneChat::Message& request)
    {
        std::lock_guard lock(mutex_); const auto& room = rooms_.at(roomId);
        const auto sender = std::find_if(room.Record.Occupants.begin(), room.Record.Occupants.end(),
            [&](const auto& member) { return member.AccountId == accountId; });
        if (sender == room.Record.Occupants.end()) { throw std::logic_error("Chat from non-member."); }
        const auto packet = Features::Chat::SceneChat::Create(static_cast<std::int16_t>(sender->Slot),
            sender->Character.CharacterName, request).Encode();
        for (const auto& peer : room.Record.Occupants)
        {
            if (request.Channel == 0 || peer.Slot == sender->Slot ||
                ((sender->Member.SignedByteAt4 == 1 || sender->Member.SignedByteAt4 == 2) &&
                    peer.Member.SignedByteAt4 == sender->Member.SignedByteAt4))
            { Deliver(room.Sinks.at(peer.Slot), {packet}); }
        }
    }
    std::optional<Characters::CharacterRecord> RoomLease::Character() const
    {
        if (!registry_) { return {}; }
        const auto room = registry_->Find(roomId_);
        if (!room) { return {}; }
        for (const auto& occupant : room->Occupants)
        { if (occupant.AccountId == accountId_) { return occupant.Character; } }
        return {};
    }
    void RoomLease::ReturnToWaiting()
    {
        if (!registry_) { throw std::logic_error("Game exit outside a room."); }
        registry_->ReturnToWaiting(roomId_, accountId_);
    }

    RoomReservation::RoomReservation(std::shared_ptr<RoomRegistry> registry, std::uint16_t roomId,
        std::uint64_t accountId, std::uint64_t characterId) noexcept :
        registry_(std::move(registry)), roomId_(roomId), accountId_(accountId), characterId_(characterId) {}

    RoomReservation::RoomReservation(RoomReservation&& other) noexcept :
        registry_(std::move(other.registry_)), roomId_(std::exchange(other.roomId_, std::uint16_t{})),
        accountId_(std::exchange(other.accountId_, std::uint64_t{})), characterId_(std::exchange(other.characterId_, std::uint64_t{})) {}

    RoomReservation& RoomReservation::operator=(RoomReservation&& other) noexcept
    {
        if (this != &other)
        {
            Abandon();
            registry_ = std::move(other.registry_);
            roomId_ = std::exchange(other.roomId_, std::uint16_t{});
            accountId_ = std::exchange(other.accountId_, std::uint64_t{});
            characterId_ = std::exchange(other.characterId_, std::uint64_t{});
        }
        return *this;
    }

    RoomReservation::~RoomReservation() { Abandon(); }

    std::uint16_t RoomReservation::RoomId() const noexcept { return roomId_; }

    RoomLease RoomReservation::Commit(RoomRecord record, RoomPacketSink hostSink, std::vector<Protocol::TcpPacket> hostPackets) &&
    {
        if (!registry_) { throw std::logic_error("Room reservation was already used."); }
        registry_->Publish(roomId_, accountId_, characterId_, std::move(record), std::move(hostSink), std::move(hostPackets));
        // Publishing succeeded; the identity now belongs to the room and the
        // host's place in it to the lease.
        RoomLease lease(std::move(registry_), roomId_, accountId_);
        roomId_ = 0;
        return lease;
    }

    void RoomReservation::Abandon() noexcept
    {
        if (registry_) { registry_->Abandon(roomId_); }
        registry_.reset();
    }

    std::shared_ptr<RoomRegistry> RoomRegistry::Create(std::size_t maxRooms, GameClock clock, SeedFactory seed,
        RoomMatchSettler settler, LocalMatchRules rules)
    {
        if (maxRooms == 0 || maxRooms > MaxRoomId) { throw std::invalid_argument("Unsupported room registry capacity."); }
        if (!clock) { clock = RoomGameSession::Clock::now; }
        if (!seed) { seed = Practice::PracticeSession::CreateRandomSeed; }
        rules.Validate();
        return std::shared_ptr<RoomRegistry>(new RoomRegistry(maxRooms, std::move(clock), std::move(seed), std::move(settler), rules));
    }

    RoomRegistry::RoomRegistry(std::size_t maxRooms, GameClock clock, SeedFactory seed, RoomMatchSettler settler, LocalMatchRules rules) :
        maxRooms_(maxRooms), clock_(std::move(clock)), seed_(std::move(seed)),
        settlements_(settler ? std::make_shared<RoomSettlementQueue>(std::move(settler)) : nullptr), rules_(rules) {}

    bool RoomRegistry::IsUsedLocked(std::uint16_t roomId) const
    {
        return rooms_.contains(roomId) || pending_.contains(roomId);
    }

    RoomOccupant* RoomRegistry::FindMemberLocked(Room& room, std::uint64_t accountId)
    {
        const auto occupant = std::find_if(room.Record.Occupants.begin(), room.Record.Occupants.end(),
            [&](const RoomOccupant& entry) { return entry.AccountId == accountId; });
        return occupant == room.Record.Occupants.end() ? nullptr : &*occupant;
    }

    void RoomRegistry::Deliver(const RoomPacketSink& sink, std::vector<Protocol::TcpPacket> packets) noexcept
    {
        if (!sink || packets.empty()) { return; }
        try { sink(std::move(packets)); }
        catch (...) {}
    }

    void RoomRegistry::Broadcast(const Room& room, const std::vector<Protocol::TcpPacket>& packets,
        std::optional<std::uint32_t> exceptSlot) noexcept
    {
        for (const auto& [slot, sink] : room.Sinks)
        {
            if (slot == exceptSlot) { continue; }
            try { Deliver(sink, packets); }
            catch (...) {}
        }
    }

    RoomReserveResult RoomRegistry::Reserve(std::uint64_t accountId, std::uint64_t characterId)
    {
        if (accountId == 0 || characterId == 0) { throw std::invalid_argument("Room owner identity is missing."); }
        std::lock_guard lock(mutex_);
        RoomReserveResult result;
        if (byAccount_.contains(accountId) || byCharacter_.contains(characterId))
        {
            result.Status = RoomReserveStatus::AlreadyInRoom;
            return result;
        }
        if (rooms_.size() + pending_.size() >= maxRooms_)
        {
            result.Status = RoomReserveStatus::RegistryFull;
            return result;
        }
        // Local policy: rotate through identities so a just-closed room number
        // is not immediately handed to a different room.
        auto roomId = nextRoomId_;
        while (IsUsedLocked(roomId)) { roomId = roomId == MaxRoomId ? 1 : static_cast<std::uint16_t>(roomId + 1); }
        pending_.emplace(roomId, Owner{accountId, characterId});
        try
        {
            byAccount_.emplace(accountId, roomId);
            byCharacter_.emplace(characterId, roomId);
        }
        catch (...)
        {
            pending_.erase(roomId);
            byAccount_.erase(accountId);
            byCharacter_.erase(characterId);
            throw;
        }
        nextRoomId_ = roomId == MaxRoomId ? 1 : static_cast<std::uint16_t>(roomId + 1);
        result.Status = RoomReserveStatus::Reserved;
        result.Reservation.emplace(RoomReservation(shared_from_this(), roomId, accountId, characterId));
        return result;
    }

    void RoomRegistry::Publish(std::uint16_t roomId, std::uint64_t accountId, std::uint64_t characterId, RoomRecord record,
        RoomPacketSink hostSink, std::vector<Protocol::TcpPacket> hostPackets)
    {
        std::lock_guard lock(mutex_);
        const auto pending = pending_.find(roomId);
        if (pending == pending_.end() || pending->second.AccountId != accountId ||
            pending->second.CharacterId != characterId)
        { throw std::logic_error("Room publication does not match its reservation."); }
        if (record.RoomId != roomId || record.SlotCount == 0 || record.HostSlot >= record.SlotCount ||
            record.State != RoomState::Waiting || record.Occupants.size() != 1 ||
            record.Occupants[0].Slot != record.HostSlot || record.Occupants[0].AccountId != accountId ||
            record.Occupants[0].Character.CharacterId != characterId || !hostSink)
        { throw std::invalid_argument("Room record does not describe the reserved host."); }
        const auto hostSlot = record.HostSlot;
        Room room;
        room.Record = std::move(record);
        room.Sinks.emplace(hostSlot, std::move(hostSink));
        const auto inserted = rooms_.emplace(roomId, std::move(room)).first;
        pending_.erase(pending);
        Deliver(inserted->second.Sinks.at(hostSlot), std::move(hostPackets));
    }

    void RoomRegistry::Abandon(std::uint16_t roomId) noexcept
    {
        std::lock_guard lock(mutex_);
        const auto pending = pending_.find(roomId);
        if (pending == pending_.end()) { return; }
        byAccount_.erase(pending->second.AccountId);
        byCharacter_.erase(pending->second.CharacterId);
        pending_.erase(pending);
    }

    RoomJoinResult RoomRegistry::Join(std::uint16_t roomId, const std::u16string& password, RoomOccupant joiner,
        RoomPacketSink sink, std::vector<Protocol::TcpPacket> joinerPrefix)
    {
        if (joiner.AccountId == 0 || joiner.Character.AccountId != joiner.AccountId ||
            joiner.Character.CharacterId == 0 || joiner.Equipment.Key != joiner.Character.CharacterId || !sink)
        { throw std::invalid_argument("Room joiner does not belong to the authenticated account."); }
        std::lock_guard lock(mutex_);
        if (byAccount_.contains(joiner.AccountId) || byCharacter_.contains(joiner.Character.CharacterId))
        { throw std::logic_error("Room joiner is already in a room."); }
        RoomJoinResult result;
        const auto entry = rooms_.find(roomId);
        if (entry == rooms_.end()) { return result; }
        auto& room = entry->second;
        auto& record = room.Record;
        if (record.State != RoomState::Waiting)
        {
            result.Status = RoomJoinStatus::Playing;
            return result;
        }
        if (password != record.Password)
        {
            result.Status = RoomJoinStatus::WrongPassword;
            return result;
        }
        std::uint32_t slot = 0;
        while (slot < record.SlotCount && record.FindSlot(slot)) { ++slot; }
        if (slot == record.SlotCount)
        {
            result.Status = RoomJoinStatus::Full;
            return result;
        }

        joiner.Slot = slot;
        const auto firstTeam = std::count_if(record.Occupants.begin(), record.Occupants.end(),
            [](const RoomOccupant& occupant) { return occupant.Member.SignedByteAt4 == 1; });
        const auto secondTeam = std::count_if(record.Occupants.begin(), record.Occupants.end(),
            [](const RoomOccupant& occupant) { return occupant.Member.SignedByteAt4 == 2; });
        joiner.Member.SignedByteAt4 = firstTeam <= secondTeam ? std::uint8_t{1} : std::uint8_t{2};
        const auto accountId = joiner.AccountId;
        const auto characterId = joiner.Character.CharacterId;
        const auto position = std::find_if(record.Occupants.begin(), record.Occupants.end(),
            [&](const RoomOccupant& occupant) { return occupant.Slot > slot; });
        record.Occupants.insert(position, std::move(joiner));
        std::vector<Protocol::TcpPacket> others;
        std::shared_ptr<RoomRegistry> self;
        try
        {
            // Everything that can fail happens before anyone observes the new
            // member. No RoomLease may be destroyed here: its release would
            // re-enter this lock.
            joinerPrefix.push_back(RoomRoster::Snapshot(record, slot).Encode());
            others.push_back(RoomRoster::SlotUpdate(record, slot).Encode());
            self = shared_from_this();
            byAccount_.emplace(accountId, roomId);
            byCharacter_.emplace(characterId, roomId);
            room.Sinks.emplace(slot, std::move(sink));
        }
        catch (...)
        {
            std::erase_if(record.Occupants, [&](const RoomOccupant& occupant) { return occupant.Slot == slot; });
            room.Sinks.erase(slot);
            byAccount_.erase(accountId);
            byCharacter_.erase(characterId);
            throw;
        }
        Broadcast(room, others, slot);
        Deliver(room.Sinks.at(slot), std::move(joinerPrefix));
        result.Status = RoomJoinStatus::Joined;
        result.Room = RoomLease(std::move(self), roomId, accountId);
        return result;
    }

    void RoomRegistry::Leave(std::uint16_t roomId, std::uint64_t accountId) noexcept
    {
        std::lock_guard lock(mutex_);
        const auto entry = rooms_.find(roomId);
        if (entry == rooms_.end()) { return; }
        auto& room = entry->second;
        auto& record = room.Record;
        const auto* member = FindMemberLocked(room, accountId);
        if (!member) { return; }
        const auto slot = member->Slot;
        byAccount_.erase(accountId);
        byCharacter_.erase(member->Character.CharacterId);
        std::erase_if(record.Occupants, [&](const RoomOccupant& occupant) { return occupant.Slot == slot; });
        room.Sinks.erase(slot);
        if (record.Occupants.empty())
        {
            rooms_.erase(entry);
            return;
        }
        std::vector<Protocol::TcpPacket> packets;
        try
        {
            packets.push_back(RoomRoster::MemberLeft(record, slot).Encode());
            if (record.HostSlot == slot)
            {
                // Local policy: the lowest remaining slot becomes host. S502
                // clears that slot's ready byte (0x009DA3C0); mirror it.
                auto& host = record.Occupants.front();
                record.HostSlot = host.Slot;
                host.Member.BooleansAt96[0] = 0;
                packets.push_back(Protocol::RoomHostChangedNotification{host.Slot}.Encode());
            }
        }
        catch (...)
        {
            // Out of memory while encoding: members keep a stale view, but the
            // departed member is gone from the server state either way.
            packets.clear();
        }
        Broadcast(room, packets);
        if (room.Game)
        {
            // Local pre-match integration policy: departures cancel the shared
            // match, never strand the remaining players at a loading barrier.
            try { ResetGameLocked(room); } catch (...) { room.Game.reset(); record.State = RoomState::Waiting; }
        }
    }

    bool RoomRegistry::SetMemberState(std::uint16_t roomId, std::uint64_t accountId, const std::array<std::uint32_t, 3>& state)
    {
        std::lock_guard lock(mutex_);
        const auto entry = rooms_.find(roomId);
        if (entry == rooms_.end()) { throw std::logic_error("Room state change for a closed room."); }
        auto& room = entry->second;
        auto* member = FindMemberLocked(room, accountId);
        if (!member) { throw std::logic_error("Room state change by a non-member."); }
        if (room.Record.State != RoomState::Waiting) { return false; }
        const auto previous = member->Member.BooleansAt96;
        member->Member.BooleansAt96 = state;
        std::vector<Protocol::TcpPacket> packets;
        try { packets.push_back(RoomRoster::SlotUpdate(room.Record, member->Slot).Encode()); }
        catch (...)
        {
            member->Member.BooleansAt96 = previous;
            throw;
        }
        Broadcast(room, packets);
        return true;
    }

    RoomStartStatus RoomRegistry::Start(std::uint16_t roomId, std::uint64_t accountId, const Protocol::TcpPacket& packet,
        std::size_t minimumPlayers, std::uint32_t durationMilliseconds, std::u16string_view trustedMapKey)
    {
        std::lock_guard lock(mutex_);
        const auto entry = rooms_.find(roomId);
        if (entry == rooms_.end()) { throw std::logic_error("Game start for a closed room."); }
        auto& room = entry->second;
        auto& record = room.Record;
        const auto* member = FindMemberLocked(room, accountId);
        if (!member) { throw std::logic_error("Game start by a non-member."); }
        if (member->Slot != record.HostSlot) { return RoomStartStatus::NotHost; }
        if (record.State != RoomState::Waiting) { return RoomStartStatus::NotWaiting; }
        if (!room.InventoryChanges.empty()) { return RoomStartStatus::MembersNotReady; }
        if (record.Occupants.size() < minimumPlayers) { return RoomStartStatus::NotEnoughPlayers; }
        // 0x0096DD20 checks the same before sending: every occupied slot
        // other than the host has its ready byte (+96) set.
        if (std::any_of(record.Occupants.begin(), record.Occupants.end(), [&](const RoomOccupant& occupant)
            { return occupant.Slot != record.HostSlot && occupant.Member.BooleansAt96[0] == 0; }))
        { return RoomStartStatus::MembersNotReady; }
        std::vector<std::uint32_t> slots;
        for (const auto& occupant : record.Occupants) { slots.push_back(occupant.Slot); }
        RoomGameSession game(slots, clock_(), seed_(), durationMilliseconds);
        game.BindMap(trustedMapKey);
        for (const auto& occupant : record.Occupants)
        {
            game.BindEquipment(occupant.Slot, occupant.Equipment.Groups[0]);
            game.BindCharacter(occupant.Slot, occupant.Character.CharacterType);
            game.BindTeam(occupant.Slot, static_cast<std::uint8_t>(occupant.Member.SignedByteAt4));
        }
        std::vector<Protocol::TcpPacket> packets{Protocol::GameTransportOptionsNotification{0, 0, 0}.Encode(), packet};
        const auto matchId = settlements_ ? CreateRoomMatchId() : std::array<std::uint8_t, 16>{};
        if (settlements_) { game.RequireSettlement(); }
        room.Game.emplace(std::move(game));
        room.FinishedGame.reset();
        room.MatchId = matchId;
        room.DurationMilliseconds = durationMilliseconds;
        room.PendingSettlement.reset();
        room.SettlementComplete = room.SettlementDelayed = false;
        room.RetrySettlementAt = {};
        record.State = RoomState::Loading;
        Broadcast(room, packets);
        return RoomStartStatus::Started;
    }

    void RoomRegistry::DeliverGameLocked(const Room& room, const std::vector<RoomGameDelivery>& deliveries)
    {
        for (const auto& delivery : deliveries)
        {
            if (delivery.OnlySlot) { Deliver(room.Sinks.at(*delivery.OnlySlot), {delivery.Packet}); }
            else { Broadcast(room, {delivery.Packet}, delivery.ExceptSlot); }
        }
    }

    void RoomRegistry::ResetGameLocked(Room& room, bool returnNotified)
    {
        auto waiting = room.Record;
        waiting.State = RoomState::Waiting;
        for (auto& occupant : waiting.Occupants) { occupant.Member.BooleansAt96 = {}; }
        // S620 must reach the old context before S117 replaces it. Normal
        // result presentation already delivered it; cancellation and loading
        // timeout need the same cleanup (RoomExit: 9EA690 / 9E1D70).
        std::vector<std::pair<std::uint32_t, std::vector<Protocol::TcpPacket>>> packets;
        for (const auto& occupant : waiting.Occupants)
        {
            std::vector<Protocol::TcpPacket> batch;
            if (!returnNotified) { batch.push_back(Protocol::GameReturnToRoomNotification{}.Encode()); }
            batch.push_back(RoomRoster::Snapshot(waiting, occupant.Slot).Encode());
            packets.emplace_back(occupant.Slot, std::move(batch));
        }
        room.Record = std::move(waiting);
        room.Game->Finish();
        room.FinishedGame = std::move(room.Game);
        room.Game.reset();
        room.PendingSettlement.reset();
        for (auto& [slot, batch] : packets) { Deliver(room.Sinks.at(slot), std::move(batch)); }
    }

    void RoomRegistry::PollLocked(Room& room, RoomGameSession::Time now)
    {
        if (!room.Game) { return; }
        const auto deliveries = room.Game->Poll(now);
        const bool returnNotified = std::any_of(deliveries.begin(), deliveries.end(), [](const RoomGameDelivery& delivery)
            { return delivery.Packet.MessageId == Protocol::GameReturnToRoomNotification::MessageId; });
        DeliverGameLocked(room, deliveries);
        if (settlements_ && room.Game->Phase() == RoomGamePhase::Result && !room.SettlementComplete)
        { SettleLocked(room, now); }
        if (room.Game->Phase() == RoomGamePhase::Returning) { ResetGameLocked(room, returnNotified); }
        else if (room.Game->Phase() == RoomGamePhase::Playing) { room.Record.State = RoomState::Playing; }
    }

    void RoomRegistry::SettleLocked(Room& room, RoomGameSession::Time now)
    {
        if (now < room.RetrySettlementAt) { return; }
        try
        {
            if (!room.PendingSettlement)
            {
                RoomMatchRequest request;
                request.MatchId = room.MatchId; request.Rules = rules_;
                request.DurationMilliseconds = room.DurationMilliseconds;
                request.TeamScores = room.Game->FinalTeamScores();
                for (const auto& occupant : room.Record.Occupants)
                { request.Members.push_back({occupant.Slot, occupant.AccountId, occupant.Character.CharacterId,
                    static_cast<std::uint8_t>(occupant.Member.SignedByteAt4), room.Game->FinalScores(occupant.Slot)}); }
                request.Validate();
                room.PendingSettlement = settlements_->Enqueue(std::move(request));
            }
            const auto& request = room.PendingSettlement->Request;
            settlements_->TrySettle(room.PendingSettlement, now);
            const auto completed = room.PendingSettlement->Result();
            if (!completed) { return; }
            const auto& result = *completed;
            auto scores = result.Scores(request).Encode();
            auto profiles = result.Profiles(request);
            auto updated = room.Record;
            for (const auto& reward : result.Rewards)
            {
                auto* member = FindMemberLocked(room, reward.Character.AccountId);
                if (!member || member->Slot != reward.Slot) { throw std::logic_error("Settlement roster changed."); }
                for (auto& occupant : updated.Occupants)
                { if (occupant.Slot == reward.Slot) { occupant.Character = reward.Character; } }
            }
            room.Game->ConfirmSettlement(now);
            room.Record = std::move(updated);
            room.SettlementComplete = true;
            Broadcast(room, {std::move(scores)});
            for (auto& [slot, packet] : profiles) { Deliver(room.Sinks.at(slot), {std::move(packet)}); }
            if (room.SettlementDelayed) { std::cerr << "Local match settlement recovered.\n"; }
        }
        catch (const std::exception&)
        {
            // Keep frozen participants/scores and the same 128-bit identity.
            // Never display uncommitted rewards or restart the match on failure.
            if (!room.SettlementDelayed) { std::cerr << "Local match settlement delayed; retrying in two seconds.\n"; }
            room.SettlementDelayed = true;
            room.RetrySettlementAt = now + std::chrono::seconds{2};
        }
    }

    void RoomRegistry::RetrySettlements()
    { if (settlements_) { settlements_->ProcessPending(clock_()); } }

    void RoomRegistry::RecoverSettlement(RoomMatchRequest request)
    {
        if (!settlements_) { throw std::logic_error("No settlement service."); }
        settlements_->Enqueue(std::move(request));
    }

    std::size_t RoomRegistry::PendingSettlementCount() const
    { return settlements_ ? settlements_->PendingCount() : 0; }

    void RoomRegistry::Poll(std::uint16_t roomId, std::uint64_t accountId)
    {
        std::lock_guard lock(mutex_);
        auto& room = rooms_.at(roomId);
        if (!FindMemberLocked(room, accountId)) { throw std::logic_error("Polling room by a non-member."); }
        PollLocked(room, clock_());
    }

    void RoomRegistry::ProcessGame(std::uint16_t roomId, std::uint64_t accountId, const Protocol::TcpPacket& packet)
    {
        std::lock_guard lock(mutex_);
        auto& room = rooms_.at(roomId);
        const auto* member = FindMemberLocked(room, accountId);
        if (!member) { throw std::runtime_error("Game packet outside an owned room."); }
        const auto slot = member->Slot;
        const auto now = clock_();
        PollLocked(room, now);
        if (!room.Game)
        {
            if (!room.FinishedGame) { throw std::runtime_error("Game packet before an owned match."); }
            // E090 live return crossed an in-flight movement. Validate against
            // the finished participants and discard without replay or reset.
            room.FinishedGame->Process(slot, packet, now);
            return;
        }
        DeliverGameLocked(room, room.Game->Process(slot, packet, now));
    }

    void RoomRegistry::ReturnToWaiting(std::uint16_t roomId, std::uint64_t accountId)
    {
        std::lock_guard lock(mutex_);
        auto& room = rooms_.at(roomId);
        if (!FindMemberLocked(room, accountId) || (!room.Game && !room.FinishedGame))
        { throw std::runtime_error("Game exit outside an owned match."); }
        if (room.Game) { ResetGameLocked(room); }
    }

    RoomListPage RoomRegistry::List(std::size_t page, std::size_t pageSize) const
    {
        if (pageSize == 0) { throw std::invalid_argument("Room list page size is zero."); }
        std::lock_guard lock(mutex_);
        RoomListPage result;
        if (page > rooms_.size() / pageSize) { return result; }
        auto room = rooms_.begin();
        std::advance(room, static_cast<std::ptrdiff_t>(page * pageSize));
        for (; room != rooms_.end() && result.Rooms.size() < pageSize; ++room)
        { result.Rooms.push_back(RoomRoster::Describe(room->second.Record)); }
        result.HasNextPage = room != rooms_.end();
        return result;
    }

    std::optional<RoomRecord> RoomRegistry::Find(std::uint16_t roomId) const
    {
        std::lock_guard lock(mutex_);
        const auto room = rooms_.find(roomId);
        if (room == rooms_.end()) { return std::nullopt; }
        return room->second.Record;
    }

    std::optional<std::uint16_t> RoomRegistry::FindByCharacter(std::uint64_t characterId) const
    {
        std::lock_guard lock(mutex_);
        const auto entry = byCharacter_.find(characterId);
        if (entry == byCharacter_.end() || !rooms_.contains(entry->second)) { return std::nullopt; }
        return entry->second;
    }

    std::size_t RoomRegistry::Count() const
    {
        std::lock_guard lock(mutex_);
        return rooms_.size();
    }
}
