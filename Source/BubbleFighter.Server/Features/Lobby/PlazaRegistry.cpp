#include "PlazaRegistry.h"
#include "Features/Chat/SceneChat.h"
#include "Protocol/RoomSlotUpdateNotification.h"
#include "Protocol/RoomMemberLeftNotification.h"
#include "Protocol/SceneDescriptorNotification.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Lobby
{
    PlazaLease::PlazaLease(std::shared_ptr<PlazaRegistry> registry, std::uint16_t scene, std::uint64_t account) noexcept :
        registry_(std::move(registry)), scene_(scene), account_(account) {}
    PlazaLease::PlazaLease(PlazaLease&& other) noexcept : registry_(std::move(other.registry_)),
        scene_(std::exchange(other.scene_, std::uint16_t{})), account_(std::exchange(other.account_, 0)) {}
    PlazaLease& PlazaLease::operator=(PlazaLease&& other) noexcept
    {
        if (this != &other) { Release(); registry_ = std::move(other.registry_);
            scene_ = std::exchange(other.scene_, std::uint16_t{}); account_ = std::exchange(other.account_, 0); }
        return *this;
    }
    PlazaLease::~PlazaLease() { Release(); }
    PlazaLease::operator bool() const noexcept { return registry_ != nullptr; }
    void PlazaLease::Release() noexcept
    { if (registry_) { auto registry = std::move(registry_); registry->Leave(scene_, account_); } }
    void PlazaLease::Move(const Protocol::PlazaMovementRequest& movement)
    { if (registry_) { registry_->Move(scene_, account_, movement); } }
    void PlazaLease::Refresh(Scenes::SceneSlot player, bool sendSnapshot)
    { if (registry_) { registry_->Refresh(scene_, account_, std::move(player), sendSnapshot); } }
    void PlazaLease::Chat(const Features::Chat::SceneChat::Message& request)
    { if (registry_) { registry_->Chat(scene_, account_, request); } }

    void PlazaRegistry::Chat(std::uint16_t sceneId, std::uint64_t account, const Features::Chat::SceneChat::Message& request)
    {
        std::lock_guard lock(mutex_);
        const auto& scene = scenes_.at(sceneId); const auto& sender = scene.Members.at(account);
        if (request.Channel != 0)
        { Deliver(sender, {Features::Chat::SceneChat::Notice(u"队伍聊天仅在房间或对战中可用。")}); return; }
        const auto packet = Features::Chat::SceneChat::Create(static_cast<std::int16_t>(sender.Slot),
            sender.Player.Character->CharacterName, request).Encode();
        for (const auto& [id, peer] : scene.Members) { (void)id; Deliver(peer, {packet}); }
    }

    Protocol::GotoRoomNotification PlazaRegistry::Snapshot(const Scene& scene, std::uint64_t account)
    {
        auto envelope = scene.Envelope;
        std::vector<Scenes::SceneSlot> slots;
        auto host = static_cast<std::uint32_t>(Capacity);
        for (const auto& [id, member] : scene.Members)
        {
            (void)id;
            slots.resize(std::max(slots.size(), static_cast<std::size_t>(member.Slot + 1)));
            slots[member.Slot] = member.Player;
            host = std::min(host, member.Slot);
        }
        envelope.Descriptor.SessionType = 3;
        envelope.Descriptor.ShortsAt32 = {static_cast<std::uint16_t>(scene.Members.size()), 0, static_cast<std::uint16_t>(Capacity)};
        envelope.ValueAt544 = host;
        return Scenes::SceneRoster::Compose(std::move(envelope), slots, scene.Members.at(account).Slot, account);
    }
    Protocol::TcpPacket PlazaRegistry::Update(const Protocol::GotoRoomNotification& snapshot)
    {
        const auto slot = snapshot.ValueAt728;
        Protocol::RoomSlotUpdateNotification m;
        m.Slot = slot; m.Member = snapshot.Members.at(slot); m.Team = snapshot.Team;
        for (const auto& r : snapshot.RecordsAt564) { if (r.Key == slot) { m.RecordsAt564.push_back(r); } }
        for (const auto& r : snapshot.RecordsAt588) { if (r.Key == m.Member.ValueAt8) { m.RecordsAt588.push_back(r); } }
        return m.Encode();
    }
    void PlazaRegistry::Deliver(const Member& member, std::vector<Protocol::TcpPacket> packets) noexcept
    { try { member.Sink(std::move(packets)); } catch (...) {} }

    PlazaLease PlazaRegistry::Enter(Protocol::GotoRoomNotification envelope, Scenes::SceneSlot player,
        std::uint64_t account, Rooms::RoomPacketSink sink)
    {
        auto self = shared_from_this();
        if (!account || !player.Character || player.Character->AccountId != account || !player.Equipment ||
            player.Equipment->Key != player.Character->CharacterId || !sink)
        { throw std::invalid_argument("Invalid authenticated plaza member."); }
        std::lock_guard lock(mutex_);
        std::uint16_t chosen = 0;
        for (const auto& [id, scene] : scenes_)
        {
            for (const auto& [other, member] : scene.Members)
            {
                if (other == account || member.Player.Character->CharacterId == player.Character->CharacterId)
                { throw std::invalid_argument("Identity already in a plaza."); }
            }
            if (!chosen && scene.Members.size() < Capacity) { chosen = id; }
        }
        if (!chosen)
        {
            for (std::uint16_t id = 1; id <= 512; ++id) { if (!scenes_.contains(id)) { chosen = id; break; } }
            if (!chosen) { throw std::runtime_error("Local plaza registry full."); }
        }
        const auto existing = scenes_.find(chosen);
        Scene candidate = existing == scenes_.end() ? Scene{std::move(envelope), {}} : existing->second;
        candidate.Envelope.Descriptor.ShortAt0 = chosen;
        std::array<bool, Capacity> used{};
        for (const auto& [id, member] : candidate.Members) { (void)id; used.at(member.Slot) = true; }
        const auto seat = static_cast<std::uint32_t>(std::find(used.begin(), used.end(), false) - used.begin());
        candidate.Members.emplace(account, Member{seat, std::move(player), std::move(sink), {}});
        const auto snapshot = Snapshot(candidate, account);
        const auto update = Update(snapshot);
        const auto descriptor = Protocol::SceneDescriptorNotification{snapshot.Descriptor}.Encode();
        std::vector<Protocol::TcpPacket> own{snapshot.Encode()};
        for (const auto& [id, member] : candidate.Members)
        {
            if (id != account && member.Movement)
            { own.push_back(Protocol::PlazaMovementNotification{static_cast<std::uint32_t>(member.Player.Character->CharacterId), *member.Movement}.Encode()); }
        }
        auto& committed = scenes_.insert_or_assign(chosen, std::move(candidate)).first->second;
        for (const auto& [id, member] : committed.Members)
        { if (id != account) { Deliver(member, {update, descriptor}); } }
        Deliver(committed.Members.at(account), std::move(own));
        return PlazaLease(std::move(self), chosen, account);
    }
    void PlazaRegistry::Leave(std::uint16_t sceneId, std::uint64_t account) noexcept
    {
        std::lock_guard lock(mutex_);
        auto found = scenes_.find(sceneId);
        if (found == scenes_.end()) { return; }
        auto& scene = found->second;
        const auto member = scene.Members.find(account);
        if (member == scene.Members.end()) { return; }
        const auto slot = member->second.Slot;
        scene.Members.erase(member);
        if (scene.Members.empty()) { scenes_.erase(found); return; }
        try
        {
            Protocol::RoomMemberLeftNotification left; left.Slot = slot; left.Team = scene.Envelope.Team;
            const auto packet = left.Encode();
            const auto snapshot = Snapshot(scene, scene.Members.begin()->first);
            const auto descriptor = Protocol::SceneDescriptorNotification{snapshot.Descriptor}.Encode();
            for (const auto& [id, peer] : scene.Members) { (void)id; Deliver(peer, {packet, descriptor}); }
        }
        catch (...) {} // The lease still vacates the seat on a failed closing connection.
    }
    void PlazaRegistry::Move(std::uint16_t sceneId, std::uint64_t account, const Protocol::PlazaMovementRequest& movement)
    {
        movement.Validate();
        std::lock_guard lock(mutex_);
        auto& scene = scenes_.at(sceneId); auto& member = scene.Members.at(account);
        const auto packet = Protocol::PlazaMovementNotification{static_cast<std::uint32_t>(member.Player.Character->CharacterId), movement}.Encode();
        member.Movement = movement;
        for (const auto& [id, peer] : scene.Members) { if (id != account) { Deliver(peer, {packet}); } }
    }
    void PlazaRegistry::Refresh(std::uint16_t sceneId, std::uint64_t account, Scenes::SceneSlot player, bool sendSnapshot)
    {
        std::lock_guard lock(mutex_);
        auto& scene = scenes_.at(sceneId); auto candidate = scene;
        auto& member = candidate.Members.at(account);
        if (!player.Character || player.Character->AccountId != account || !player.Equipment ||
            player.Equipment->Key != player.Character->CharacterId ||
            player.Character->CharacterId != member.Player.Character->CharacterId)
        { throw std::invalid_argument("Plaza refresh identity mismatch."); }
        member.Player = std::move(player);
        const auto snapshot = Snapshot(candidate, account); const auto update = Update(snapshot);
        std::vector<Protocol::TcpPacket> own;
        if (sendSnapshot)
        {
            member.Movement.reset(); own.push_back(snapshot.Encode());
            for (const auto& [id, peer] : candidate.Members)
            { if (id != account && peer.Movement) { own.push_back(Protocol::PlazaMovementNotification{
                static_cast<std::uint32_t>(peer.Player.Character->CharacterId), *peer.Movement}.Encode()); } }
        }
        scene = std::move(candidate);
        for (const auto& [id, peer] : scene.Members) { if (id != account) { Deliver(peer, {update}); } }
        if (sendSnapshot) { Deliver(scene.Members.at(account), std::move(own)); }
    }
}
