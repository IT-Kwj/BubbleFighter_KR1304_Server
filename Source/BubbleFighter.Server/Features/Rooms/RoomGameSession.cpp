#include "RoomGameSession.h"
#include "Protocol/ClientToServerMessage0x025F.h"
#include "Protocol/ClientToServerMessage0x0260.h"
#include "Protocol/ClientToServerMessage0x07ED.h"
#include "Protocol/ServerToClientMessage0x025E.h"
#include "Protocol/GameCountdownNotification.h"
#include "Protocol/GameStartNotification.h"
#include "Protocol/GameMovementNotification.h"
#include "Protocol/GameShotNotification.h"
#include "Protocol/GameExtendedShotNotification.h"
#include "Protocol/GameActorDeathNotification.h"
#include "Protocol/GameActorRespawnWaitNotification.h"
#include "Protocol/GameRespawnRequest.h"
#include "Protocol/GameRespawnNotification.h"
#include "Protocol/GameRespawnCompletionNotification.h"
#include "Protocol/GameHitNotification.h"
#include "Protocol/GameDamagerRemoveNotification.h"
#include "Protocol/GameActorDamageResultNotification.h"
#include "Protocol/GameBubbleExitNotification.h"
#include "Protocol/GameBubbleSettledNotification.h"
#include "Protocol/GameWeaponSwitchNotification.h"
#include "Protocol/GameBubbleShotPopRequest.h"
#include "Protocol/GameBubbleActionStartNotification.h"
#include "Protocol/GameBubbleActionEndNotification.h"
#include "Protocol/GameBubbleActionCompleteNotification.h"
#include "Protocol/ClientToServerMessage0x800B.h"
#include "Protocol/GameMeleeAttackNotification.h"
#include "Protocol/GameWeaponImpactNotification.h"
#include "Protocol/GameGrenadeDetonateNotification.h"
#include "Protocol/GameStompStartNotification.h"
#include "Protocol/GameStompHitNotification.h"
#include "Protocol/ClientToServerMessage0x800C.h"
#include "Protocol/ClientToServerMessage0x800D.h"
#include "Protocol/ClientToServerMessage0x800E.h"
#include "Protocol/ClientToServerMessage0x802D.h"
#include "Protocol/GameBubbleContactPopRequest.h"
#include "Protocol/GameBubbleDeflectNotification.h"
#include "Protocol/GameBubblePopNotification.h"
#include "Protocol/GameBubbleRescueRequest.h"
#include "Protocol/GameBubbleRescueNotification.h"
#include "Protocol/GameBubbleRescueEffectNotification.h"
#include "Protocol/GameScoreUpdateNotification.h"
#include "Protocol/GameOverNotification.h"
#include "Protocol/GameResultNotification.h"
#include "Protocol/GameResultPresentationNotification.h"
#include "Protocol/GameReturnToRoomNotification.h"
#include "Features/Battle/WeaponCatalog.h"
#include "Features/Battle/CombatWeaponCatalog.h"
#include "Features/Inventory/ItemCatalog.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Rooms
{
    RoomGameSession::RoomGameSession(std::span<const std::uint32_t> slots, Time now, std::uint32_t seed,
        std::uint32_t durationMilliseconds) : lastObserved_(now), deadline_(now + LoadLimit), seed_(seed), duration_(durationMilliseconds)
    {
        if (slots.empty() || seed == 0 || seed >= 0x7FFFFFFF || duration_ == 0)
        { throw std::invalid_argument("Invalid room game initialization."); }
        for (auto slot : slots)
        {
            ActorId(slot);
            if (!members_.emplace(slot, Member{}).second) { throw std::invalid_argument("Duplicate game slot."); }
            members_.at(slot).NextDamagerKey = slot * 100000U;
        }
    }

    void RoomGameSession::BindEquipment(std::uint32_t slot, std::span<const std::uint32_t> items)
    {
        if (phase_ != RoomGamePhase::Loading || members_.at(slot).Loaded)
        { throw std::runtime_error("Cannot replace equipment after room loading."); }
        std::array<std::uint32_t, 6> weapons{2006, 1005, 7001, 5033, 0, 0};
        std::uint32_t occupied = 0;
        for (auto item : items)
        {
            const auto* definition = Inventory::ItemCatalog::Find(item);
            if (!definition) { throw std::runtime_error("Unknown trusted room equipment."); }
            // 817680 builds the slot map; 817E80 reads entries 18..21.
            for (std::uint32_t index = 0; index < 4; ++index)
            {
                const auto bit = 1U << (index + 18);
                if ((definition->SlotMask & bit) == 0) { continue; }
                if ((occupied & bit) != 0) { throw std::runtime_error("Conflicting trusted room weapons."); }
                occupied |= bit;
                const auto* weapon = Battle::WeaponCatalog::FindEquipmentItem(item);
                weapons[index] = weapon && weapon->WeaponSlotIndex == index ? weapon->WeaponId : 0;
            }
        }
        auto& member = members_.at(slot);
        member.Weapons = weapons;
        member.EquipmentItems.assign(items.begin(), items.end());
        RefreshInitialMaximumHealth(member);
    }

    void RoomGameSession::BindTeam(std::uint32_t slot, std::uint8_t team)
    {
        if (phase_ != RoomGamePhase::Loading || members_.at(slot).Loaded || (team != 1 && team != 2))
        { throw std::runtime_error("Invalid trusted room team binding."); }
        members_.at(slot).Team = team;
    }

    void RoomGameSession::BindMap(std::u16string_view trustedMapKey)
    {
        if (phase_ != RoomGamePhase::Loading || std::any_of(members_.begin(), members_.end(),
            [](const auto& entry) { return entry.second.Loaded.has_value(); }))
        { throw std::runtime_error("Cannot replace room map after loading."); }
        scene_ = Battle::CombatScene::Find(trustedMapKey);
    }

    void RoomGameSession::BindCharacter(std::uint32_t slot, std::uint32_t trustedCharacterType)
    {
        auto& member = members_.at(slot);
        if (phase_ != RoomGamePhase::Loading || member.Loaded)
        { throw std::runtime_error("Cannot replace fighter attributes after room loading."); }
        // Current room policy is channel 60 / ordinary mode 1. Preserve all
        // variants for exact resource comparisons; selection is below.
        for (std::size_t i = 0; i < member.BaseAttributes.size(); ++i)
        { member.BaseAttributes[i] = Battle::FighterAttributeCatalog::Resolve(trustedCharacterType, 60, 1,
            static_cast<Battle::FighterAttributeVariant>(i)); }
        member.CharacterType = trustedCharacterType;
        RefreshInitialMaximumHealth(member);
    }

    const std::array<std::optional<Battle::FighterBaseAttributes>, 3>& RoomGameSession::BaseAttributes(std::uint32_t slot) const
    { return members_.at(slot).BaseAttributes; }

    const std::optional<Battle::FighterBaseAttributes>& RoomGameSession::SelectedBaseAttributes(std::uint32_t slot) const
    {
        // CA3A60 reads Experimental.arenaTestSeason.use=false from the pinned
        // resource. 47F1A0 therefore keeps actor+860 (normal table). Current
        // RoomCreation rejects nonzero mode flags, including BubbleCup 0x10.
        // This accessor deliberately remains the unmodified base table.
        return members_.at(slot).BaseAttributes[static_cast<std::size_t>(Battle::FighterAttributeVariant::Normal)];
    }

    void RoomGameSession::RefreshInitialMaximumHealth(Member& member)
    {
        member.InitialMaximumHp.reset();
        const auto& base = member.BaseAttributes[static_cast<std::size_t>(Battle::FighterAttributeVariant::Normal)];
        if (!member.CharacterType || !base) { return; }
        const auto equipment = Battle::FighterMaximumHealth::Equipment(*member.CharacterType, member.EquipmentItems, 60);
        // Current ordinary mode 1 / modeFlags=0 has no initial actor+1444
        // multiplier. character.lua returns the supplied 1.0 outside the arena
        // and test-season branches. Runtime effects must not reuse this seed.
        member.InitialMaximumHp = Battle::FighterMaximumHealth::Ordinary(base->Hp, equipment, 1.0F, 1.0F);
    }

    std::optional<float> RoomGameSession::InitialMaximumHealth(std::uint32_t slot) const
    { return members_.at(slot).InitialMaximumHp; }

    void RoomGameSession::ResetMovement(Member& member) noexcept
    {
        member.Movement.Clear();
        ++member.MovementEpoch;
    }

    std::uint32_t RoomGameSession::ActorId(std::uint32_t slot)
    {
        if (slot >= 16) { throw std::invalid_argument("Unsupported game slot."); }
        // BFCLIENT02 6D2040 resets and consumes the slot's first object id.
        return 1000000U + slot * 100000U;
    }

    void RoomGameSession::Observe(Time now)
    {
        if (now < lastObserved_) { throw std::runtime_error("Room game clock moved backwards."); }
        lastObserved_ = now;
    }

    void RoomGameSession::CheckActor(std::uint32_t slot, std::uint32_t actor) const
    {
        if (!members_.contains(slot) || actor != ActorId(slot))
        { throw std::runtime_error("Game actor does not belong to the authenticated room slot."); }
    }

    bool RoomGameSession::Handles(std::uint16_t id) noexcept
    {
        using namespace Protocol;
        return id == GameLoadingCompleteRequest::MessageId || id == ClientToServerMessage0x0243::MessageId ||
            id == ClientToServerMessage0x025F::MessageId || id == ClientToServerMessage0x0260::MessageId ||
            id == ClientToServerMessage0x07ED::MessageId || id == GameMovementNotification::MessageId ||
            id == GameShotNotification::MessageId || id == GameExtendedShotNotification::MessageId ||
            id == GameActorDeathNotification::MessageId || id == GameActorRespawnWaitNotification::MessageId ||
            id == GameRespawnRequest::MessageId || id == GameRespawnCompletionNotification::MessageId ||
            id == GameHitNotification::MessageId || id == GameDamagerRemoveNotification::MessageId ||
            id == GameActorDamageResultNotification::MessageId || id == GameBubbleExitNotification::MessageId ||
            id == GameBubbleSettledNotification::MessageId || id == GameWeaponSwitchNotification::MessageId ||
            id == GameBubbleActionStartNotification::MessageId || id == GameBubbleActionEndNotification::MessageId ||
            id == GameBubbleActionCompleteNotification::MessageId ||
            id == GameMeleeAttackNotification::MessageId ||
            id == GameWeaponImpactNotification::MessageId || id == GameGrenadeDetonateNotification::MessageId ||
            id == GameStompStartNotification::MessageId || id == GameStompHitNotification::MessageId ||
            id == ClientToServerMessage0x800B::MessageId || id == ClientToServerMessage0x800C::MessageId ||
            id == ClientToServerMessage0x800D::MessageId || id == ClientToServerMessage0x800E::MessageId ||
            id == ClientToServerMessage0x802D::MessageId ||
            id == GameBubbleShotPopRequest::MessageId || id == GameBubbleContactPopRequest::MessageId ||
            id == GameBubbleDeflectNotification::MessageId || id == GameBubblePopNotification::MessageId ||
            id == GameBubbleRescueRequest::MessageId || id == GameBubbleRescueNotification::MessageId ||
            id == GameBubbleRescueEffectNotification::MessageId || id == GameReportRequest::MessageId;
    }

    std::vector<RoomGameDelivery> RoomGameSession::Process(std::uint32_t slot, const Protocol::TcpPacket& packet, Time now)
    {
        using namespace Protocol;
        Observe(now);
        auto found = members_.find(slot);
        if (found == members_.end()) { throw std::runtime_error("Game message by a nonparticipant."); }
        const bool awaitingCommit = phase_ == RoomGamePhase::Result && settlementRequired_ && !settlementConfirmed_;
        if (phase_ != RoomGamePhase::Returning && !awaitingCommit && now >= deadline_)
        { throw std::runtime_error("Game message after phase deadline; poll transition first."); }
        auto& member = found->second;
        if (packet.MessageId == GameReportRequest::MessageId)
        {
            const auto report = GameReportRequest::Decode(packet);
            // Late reports from a closed game are parsed but cannot reopen it.
            if (phase_ == RoomGamePhase::Returning) { return {}; }
            if (report.RandomSeed != seed_) { throw std::runtime_error("Room report belongs to another start."); }
            if (member.EndingReport)
            {
                if (*member.EndingReport != report) { throw std::runtime_error("Changed duplicate room game report."); }
                return {};
            }
            if (phase_ == RoomGamePhase::Result || phase_ == RoomGamePhase::ResultPresentation)
            { return {}; } // A late first report cannot change the result.
            if (phase_ != RoomGamePhase::AwaitingReports)
            { throw std::runtime_error("Room report outside the server-initiated game-over window."); }
            member.EndingReport = report;
            if (std::all_of(members_.begin(), members_.end(), [](const auto& entry) { return entry.second.EndingReport.has_value(); }))
            { deadline_ = std::max(now, gameOverAt_ + GameOverDisplay); }
            return {}; // Public seed and 29 opaque counters authorize no rewards.
        }
        if (packet.MessageId == ClientToServerMessage0x0243::MessageId)
        {
            auto data = ClientToServerMessage0x0243::Decode(packet);
            if (Ended()) { return {}; }
            if (member.LoadingData)
            {
                if (*member.LoadingData != data) { throw std::runtime_error("Changed room loading snapshot."); }
            }
            else
            {
                if (phase_ != RoomGamePhase::Loading) { throw std::runtime_error("Late room loading snapshot."); }
                member.LoadingData = std::move(data);
            }
            return {};
        }
        if (packet.MessageId == ClientToServerMessage0x025F::MessageId)
        {
            CheckActor(slot, ClientToServerMessage0x025F::Decode(packet).ValueAt0);
            if (Ended()) { return {}; }
            if (!member.ActorReported && phase_ != RoomGamePhase::Loading)
            { throw std::runtime_error("Late room actor report."); }
            member.ActorReported = true;
            return {};
        }
        if (packet.MessageId == GameLoadingCompleteRequest::MessageId)
        {
            auto report = GameLoadingCompleteRequest::Decode(packet);
            // 6DC970: ordinary mode 1 emits 0 or the per-player 3000 branch.
            if (report.ValueAt8 || report.ValueAt12 || (report.DelayMilliseconds != 0 && report.DelayMilliseconds != 3000))
            { throw std::runtime_error("Unsupported ordinary-room loading report."); }
            if (Ended()) { return {}; }
            if (member.Loaded)
            {
                if (*member.Loaded != report) { throw std::runtime_error("Changed duplicate room loading report."); }
                return {};
            }
            if (phase_ != RoomGamePhase::Loading) { throw std::runtime_error("Late room loading completion."); }
            std::vector<RoomGameDelivery> output{{ServerToClientMessage0x025E{0}.Encode(), slot, {}}};
            const bool othersLoaded = std::all_of(members_.begin(), members_.end(), [slot](const auto& entry)
            { return entry.first == slot || entry.second.Loaded.has_value(); });
            if (othersLoaded)
            { output.push_back({GameCountdownNotification{static_cast<std::uint32_t>(Countdown.count())}.Encode(), {}, {}}); }
            member.Loaded = report;
            if (othersLoaded) { phase_ = RoomGamePhase::Countdown; deadline_ = now + Countdown; }
            return output;
        }
        if (packet.MessageId == ClientToServerMessage0x0260::MessageId)
        { ClientToServerMessage0x0260::Decode(packet); return {}; }
        if (packet.MessageId == ClientToServerMessage0x07ED::MessageId)
        { ClientToServerMessage0x07ED::Decode(packet); return {}; }
        if (packet.MessageId == GameMovementNotification::MessageId)
        {
            const auto move = GameMovementNotification::Decode(packet);
            CheckActor(slot, move.ActorObjectId);
            for (auto coordinate : move.Position)
            { if (!std::isfinite(coordinate)) { throw std::runtime_error("Non-finite room movement."); } }
            // Remote actors exist only after loading. Never queue old positions into a new map.
            if (phase_ != RoomGamePhase::Playing) { return {}; }
            if (member.ActorPhase != RoomActorPhase::Alive && member.ActorPhase != RoomActorPhase::Bubble) { return {}; }
            member.Movement.Record(now, move.Position);
            return {{move.Encode(), {}, slot}};
        }
        if (packet.MessageId == GameHitNotification::MessageId || packet.MessageId == GameDamagerRemoveNotification::MessageId)
        { return ProcessHit(slot, packet); }
        if (packet.MessageId == GameActorDamageResultNotification::MessageId)
        { return ProcessDamageResult(slot, packet); }
        if (packet.MessageId == GameBubbleSettledNotification::MessageId)
        {
            const auto settled = GameBubbleSettledNotification::Decode(packet);
            CheckActor(slot, settled.ActorObjectId);
            if (Ended() || phase_ != RoomGamePhase::Playing ||
                member.ActorPhase != RoomActorPhase::Bubble || member.BubbleSettled) { return {}; }
            // Echo to the owner too: native +552 requests state 301. Merely
            // recording Bubble on the server leaves every client rolling.
            std::vector<RoomGameDelivery> output{{packet, {}, {}}};
            member.BubbleSettled = true;
            return output;
        }
        if (packet.MessageId == GameWeaponSwitchNotification::MessageId)
        {
            const auto change = GameWeaponSwitchNotification::Decode(packet);
            CheckActor(slot, change.ActorObjectId);
            if (change.WeaponIndex >= member.Weapons.size() || change.SelectValue > 1 || change.ValueAt20 > 1)
            { throw std::runtime_error("Invalid ordinary weapon switch fields."); }
            if (Ended() || member.ActorPhase != RoomActorPhase::Alive || change.WeaponId == 0 ||
                member.Weapons[change.WeaponIndex] != change.WeaponId) { return {}; }
            return {{packet, {}, {}}};
        }
        if (packet.MessageId == GameBubbleActionStartNotification::MessageId ||
            packet.MessageId == GameBubbleActionEndNotification::MessageId ||
            packet.MessageId == GameBubbleActionCompleteNotification::MessageId)
        { return ProcessBubbleAction(slot, packet); }
        if (packet.MessageId == GameMeleeAttackNotification::MessageId)
        { return ProcessMeleeAttack(slot, packet); }
        if (packet.MessageId == GameWeaponImpactNotification::MessageId)
        { return ProcessWeaponImpact(slot, packet); }
        if (packet.MessageId == GameGrenadeDetonateNotification::MessageId)
        { return ProcessGrenadeDetonate(slot, packet); }
        if (packet.MessageId == GameStompStartNotification::MessageId ||
            packet.MessageId == GameStompHitNotification::MessageId)
        { return ProcessStomp(slot, packet); }
        if (packet.MessageId == ClientToServerMessage0x800B::MessageId ||
            packet.MessageId == ClientToServerMessage0x800C::MessageId ||
            packet.MessageId == ClientToServerMessage0x800D::MessageId ||
            packet.MessageId == ClientToServerMessage0x800E::MessageId ||
            packet.MessageId == ClientToServerMessage0x802D::MessageId)
        { return ProcessActorAction(slot, packet); }
        if (packet.MessageId == GameBubbleExitNotification::MessageId ||
            packet.MessageId == GameBubbleShotPopRequest::MessageId || packet.MessageId == GameBubbleContactPopRequest::MessageId ||
            packet.MessageId == GameBubbleDeflectNotification::MessageId || packet.MessageId == GameBubblePopNotification::MessageId ||
            packet.MessageId == GameBubbleRescueRequest::MessageId || packet.MessageId == GameBubbleRescueNotification::MessageId ||
            packet.MessageId == GameBubbleRescueEffectNotification::MessageId)
        { return ProcessBubble(slot, packet); }
        // Ordinary guns and equipped grenades share the GoDamager key range.
        if (packet.MessageId == GameShotNotification::MessageId || packet.MessageId == GameExtendedShotNotification::MessageId)
        {
            const auto extended = packet.MessageId == GameExtendedShotNotification::MessageId ?
                GameExtendedShotNotification::Decode(packet) : GameExtendedShotNotification{};
            const auto shot = packet.MessageId == GameShotNotification::MessageId ?
                GameShotNotification::Decode(packet).Shot : extended.Shot;
            CheckActor(slot, shot.ActorObjectId);
            // The explicit geometry checks are shared with the existing codec's field semantics.
            if (!shot.HasFiniteCoordinates() || shot.WeaponIndex >= 6)
            { throw std::runtime_error("Unsupported room shot geometry or weapon index."); }
            if (Ended()) { return {}; }
            if (phase_ != RoomGamePhase::Playing) { throw std::runtime_error("Shot before shared game start."); }
            // In-flight shots during this player's death cannot create new
            // damage objects. Other participants retain their own live state.
            if (member.ActorPhase != RoomActorPhase::Alive) { return {}; }
            const auto* weapon = Battle::WeaponCatalog::Find(member.Weapons[shot.WeaponIndex]);
            const bool ordinaryGun = weapon && (weapon->MasterId == 1000 || weapon->MasterId == 2000 ||
                weapon->MasterId == 3000 || weapon->MasterId == 4000);
            // E092 verifies all four master families against current resources:
            // their category is unchanged and isExplosive is false. Alternate
            // resources and reinforcement lists need a separate derivation.
            const bool grenade = weapon && weapon->MasterId == 5000 && shot.WeaponIndex == 3;
            const bool tracked = (ordinaryGun || grenade) && extended.ValueAt40 == 0 && extended.ValuesAt44.empty();
            const auto count = tracked && weapon->IsSpread ? weapon->SpreadCount : 1U;
            const auto first = slot * 100000U;
            if (count == 0 || count > 100000U || shot.ShotSequence < first ||
                shot.ShotSequence >= first + 100000U || count > first + 100000U - shot.ShotSequence)
            { throw std::runtime_error("Shot damager keys escape the authenticated room slot."); }
            // Gaps are legal: other native producers consume this same counter.
            // The per-match high-water mark prevents replay after a key retires.
            if (shot.ShotSequence < member.NextDamagerKey) { return {}; }
            if (scene_)
            {
                std::erase_if(member.Damagers, [&](const auto& entry)
                    { return now - entry.second.Created > DamageResultLifetime; });
            }
            // Explicit local memory bound, not a recovered native ammo limit.
            if (tracked && member.Damagers.size() + count > 4096)
            { throw std::runtime_error("Too many active room damage objects."); }
            // The original thrower already consumes its shot locally (origin 1).
            // Echoing a grenade creates a second body with the same damager key.
            std::vector<RoomGameDelivery> deliveries{{packet, {}, grenade ? std::optional{slot} : std::nullopt}};
            if (tracked)
            {
                const auto origin = Battle::CombatGeometry::DecodeBinary16(shot.Position);
                const auto source = member.Movement.Latest(now, std::chrono::seconds{5});
                std::array<std::uint64_t, 16> epochs{};
                for (const auto& [targetSlot, targetMember] : members_) { epochs[targetSlot] = targetMember.MovementEpoch; }
                for (std::uint32_t i = 0; i < count; ++i)
                { member.Damagers.emplace(shot.ShotSequence + i, Damager{weapon->WeaponId, 0, origin, now, source, epochs,
                    grenade ? DamagerKind::Grenade : DamagerKind::Bullet}); }
            }
            member.NextDamagerKey = shot.ShotSequence + count;
            return deliveries;
        }
        return ProcessActor(slot, packet);
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessHit(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        const bool isHit = packet.MessageId == GameHitNotification::MessageId;
        const auto hit = isHit ? GameHitNotification::Decode(packet) : GameHitNotification{};
        const auto key = isHit ? hit.DamagerKey : GameDamagerRemoveNotification::Decode(packet).DamagerKey;
        if (key < slot * 100000U || key >= (slot + 1U) * 100000U)
        { throw std::runtime_error("Damage object does not belong to the authenticated room slot."); }
        if (isHit)
        {
            for (auto coordinate : hit.Position)
            { if ((coordinate & 0x7C00U) == 0x7C00U) { throw std::runtime_error("Non-finite room hit position."); } }
        }
        if (Ended()) { return {}; }
        if (phase_ != RoomGamePhase::Playing) { throw std::runtime_error("Hit before shared game start."); }
        auto& member = members_.at(slot);
        auto found = member.Damagers.find(key);
        // Missing, retired, unsupported, and repeated reports cannot create a
        // damage object or revive a retired key. Native missing lookup is inert.
        if (found == member.Damagers.end()) { return {}; }
        if (!isHit)
        {
            std::vector<RoomGameDelivery> deliveries{{packet, {}, {}}};
            member.Damagers.erase(found);
            return deliveries;
        }
        if (found->second.Kind != DamagerKind::Bullet) { return {}; }
        std::optional<std::uint32_t> target;
        for (const auto& [candidate, state] : members_)
        {
            (void)state;
            if (hit.TargetObjectId == ActorId(candidate)) { target = candidate; break; }
        }
        // Map/NPC identity has not been established by this room. Retire an
        // owned non-retained projectile without forwarding an unknown target.
        if (!target)
        {
            if (hit.KeepDamagerValue != 0) { return {}; }
            std::vector<RoomGameDelivery> deliveries{{GameDamagerRemoveNotification{key}.Encode(), {}, {}}};
            member.Damagers.erase(found);
            return deliveries;
        }
        const auto bit = static_cast<std::uint16_t>(1U << *target);
        // A pre-transition report must not be applied to a revived actor at
        // the same object ID, even if fresh coordinates have already arrived.
        if (found->second.TargetMovementEpochs[*target] != members_.at(*target).MovementEpoch) { return {}; }
        if (scene_)
        {
            const auto& damager = found->second;
            const auto targetPosition = members_.at(*target).Movement.Latest(lastObserved_, std::chrono::seconds{5});
            // Local broad consistency envelope, NOT the native bone hitboxes.
            // Receipt timestamps cannot be rewound by the client's packed time.
            constexpr double reportEnvelopeSquared = 64.0 * 64.0;
            const auto point = Battle::CombatGeometry::DecodeBinary16(hit.Position);
            if (lastObserved_ - damager.Created > DamageResultLifetime || !damager.ReportedSourcePosition || !targetPosition ||
                Battle::CombatGeometry::DistanceSquared(damager.Origin, *damager.ReportedSourcePosition) > reportEnvelopeSquared ||
                Battle::CombatGeometry::DistanceSquared(point, *targetPosition) > reportEnvelopeSquared)
            { return {}; }
            const auto obstruction = scene_->FirstObstruction(damager.Origin, point, Battle::CombatSceneQuery::Bullet);
            // Half-float contact coordinates are quantized. Exclude only walls
            // at least two world units before the reported contact point.
            const auto distance = std::sqrt(Battle::CombatGeometry::DistanceSquared(damager.Origin, point));
            if (obstruction && (1.0 - *obstruction) * distance > 2.0) { return {}; }
        }
        if ((found->second.HitSlots & bit) != 0)
        {
            if (hit.KeepDamagerValue != 0) { return {}; }
            std::vector<RoomGameDelivery> deliveries{{GameDamagerRemoveNotification{key}.Encode(), {}, {}}};
            member.Damagers.erase(found);
            return deliveries;
        }
        std::vector<RoomGameDelivery> deliveries{{packet, {}, {}}};
        auto& victim = members_.at(*target);
        if (victim.ActorPhase == RoomActorPhase::Alive)
        {
            victim.DamageSources.insert_or_assign({ActorId(slot), found->second.WeaponId},
                lastObserved_ + DamageResultLifetime);
        }
        if (hit.KeepDamagerValue == 0) { member.Damagers.erase(found); }
        else { found->second.HitSlots |= bit; }
        // The original clients apply their weapon/target logic. This relay
        // does not infer HP, confirm physical collision, score, or grant rewards.
        return deliveries;
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessDamageResult(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        const auto report = Protocol::GameActorDamageResultNotification::Decode(packet);
        CheckActor(slot, report.ActorObjectId);
        std::optional<std::uint32_t> sourceSlot;
        for (const auto& [candidate, state] : members_)
        {
            (void)state;
            if (report.SourceObjectId == ActorId(candidate)) { sourceSlot = candidate; }
        }
        if (!sourceSlot) { throw std::runtime_error("Room damage source is not a participant."); }
        for (auto bits : report.VectorBits)
        { if ((bits & 0x7F800000U) == 0x7F800000U) { throw std::runtime_error("Non-finite room damage result vector."); } }
        // 6E3B80 emits byte-sized flags as uint32 and a zero final field.
        // Preserve the complete time word, including its unspecified high byte.
        if (report.DirectDeathValue > 1 || report.FullHealthHitValue > 1 || report.ValueAt36 != 0)
        { throw std::runtime_error("Unsupported ordinary-room damage result flags."); }
        if (Ended()) { return {}; }
        if (phase_ != RoomGamePhase::Playing)
        { throw std::runtime_error("Damage result before shared game start."); }
        auto& member = members_.at(slot);
        if (member.ActorPhase != RoomActorPhase::Alive) { return {}; }
        // A result cannot authorize an unsolicited victim, weapon, or source.
        // Keep unmatched reports inert, just like an absent native hit object.
        const auto pending = member.DamageSources.find({report.SourceObjectId, report.WeaponId});
        if (pending == member.DamageSources.end()) { return {}; }
        if (pending->second <= lastObserved_)
        { member.DamageSources.erase(pending); return {}; }
        std::vector<RoomGameDelivery> deliveries{{packet, {}, {}}};
        const bool enemies = member.Team != 0 && members_.at(*sourceSlot).Team != 0 &&
            member.Team != members_.at(*sourceSlot).Team;
        if (enemies)
        { AppendScore(deliveries, report.DirectDeathValue ? ScoreEvent::Pang : ScoreEvent::Bubble, *sourceSlot, slot); }
        else if (report.DirectDeathValue) { AppendScore(deliveries, ScoreEvent::Death, slot, slot); }
        member.ActorPhase = report.DirectDeathValue ? RoomActorPhase::Down : RoomActorPhase::Bubble;
        ResetMovement(member);
        ClearBubbleLinks(slot);
        member.RespawnCompleted = false;
        member.DamageSources.clear();
        return deliveries;
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessActor(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        std::uint32_t actor = 0;
        TcpPacket response;
        if (packet.MessageId == GameActorDeathNotification::MessageId)
        { actor = GameActorDeathNotification::Decode(packet).ActorObjectId; response = packet; }
        else if (packet.MessageId == GameActorRespawnWaitNotification::MessageId)
        { actor = GameActorRespawnWaitNotification::Decode(packet).ActorObjectId; response = packet; }
        else if (packet.MessageId == GameRespawnRequest::MessageId)
        {
            const auto request = GameRespawnRequest::Decode(packet);
            actor = request.Parameters.ActorObjectId;
            // E091: mode 1 reaches 515C55 with absent optional parameters.
            // The alternative selector at 9EC0A0 requires mode 12. The current
            // resource's ordinary itemResetOnRespawn is zero (6E3730).
            if (request.Parameters.ValueAt4 || request.Parameters.ValueAt12 || request.ResetItemsValue != 0)
            { throw std::runtime_error("Unsupported ordinary-room respawn parameters."); }
            // Explicit unrewarded local policy: preserve the member +304 zero
            // baseline. This does not claim original life-count semantics.
            response = GameRespawnNotification{request.Parameters, 0, 0}.Encode();
        }
        else if (packet.MessageId == GameRespawnCompletionNotification::MessageId)
        {
            const auto report = GameRespawnCompletionNotification::Decode(packet);
            actor = report.Parameters.ActorObjectId;
            if (report.Parameters.ValueAt4 || report.Parameters.ValueAt12 || report.ByteAt20 != 0 || report.ValueAt21 != 0)
            { throw std::runtime_error("Unsupported ordinary-room respawn completion."); }
            response = packet;
        }
        else { throw std::runtime_error("Unsupported room game message."); }
        CheckActor(slot, actor);
        // Keep parsing/ownership checks after returning, but never resurrect a
        // finished match or replay a notification into its waiting room.
        if (Ended()) { return {}; }
        if (phase_ != RoomGamePhase::Playing)
        { throw std::runtime_error("Actor lifecycle before shared game start."); }
        auto& member = members_.at(slot);
        auto next = member.ActorPhase;
        bool completed = member.RespawnCompleted;
        if (packet.MessageId == GameActorDeathNotification::MessageId)
        {
            if (next != RoomActorPhase::Alive && next != RoomActorPhase::Bubble) { return {}; }
            next = RoomActorPhase::Down;
            completed = false;
        }
        else if (packet.MessageId == GameActorRespawnWaitNotification::MessageId)
        {
            if (next == RoomActorPhase::Alive || next == RoomActorPhase::Bubble)
            { throw std::runtime_error("Room respawn wait without death."); }
            if (next != RoomActorPhase::Down) { return {}; }
            next = RoomActorPhase::Waiting;
        }
        else if (packet.MessageId == GameRespawnRequest::MessageId)
        {
            if (next == RoomActorPhase::AwaitingCompletion) { return {}; }
            if (next != RoomActorPhase::Waiting) { throw std::runtime_error("Room respawn request outside death/wait sequence."); }
            next = RoomActorPhase::AwaitingCompletion;
        }
        else
        {
            if (next == RoomActorPhase::Alive && completed) { return {}; }
            if (next != RoomActorPhase::AwaitingCompletion) { throw std::runtime_error("Unsolicited room respawn completion."); }
            next = RoomActorPhase::Alive;
            completed = true;
        }
        // All clients apply death/wait/completion to the same scene actor.
        // S632 updates scene membership too; its native ownership predicate
        // restricts the follow-up completion sender. Never accept a peer's ID.
        std::vector<RoomGameDelivery> deliveries{{std::move(response), {}, {}}};
        if (packet.MessageId == GameActorDeathNotification::MessageId)
        { AppendScore(deliveries, ScoreEvent::Death, slot, slot); }
        member.ActorPhase = next;
        if (next == RoomActorPhase::Down || next == RoomActorPhase::Alive)
        { ResetMovement(member); }
        ClearBubbleLinks(slot);
        member.RespawnCompleted = completed;
        member.DamageSources.clear();
        return deliveries;
    }


    void RoomGameSession::ClearBubbleLinks(std::uint32_t slot) noexcept
    {
        members_.at(slot).BubbleSettled = false;
        members_.at(slot).Action.reset();
        members_.at(slot).Stomp.reset();
        for (auto& [other, member] : members_)
        {
            for (auto* pending : {&member.Pop, &member.Rescue, &member.RescueEffect})
            {
                if (other == slot || (*pending && (*pending)->SourceSlot == slot)) { pending->reset(); }
            }
        }
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessMeleeAttack(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        const auto attack = Protocol::GameMeleeAttackNotification::Decode(packet);
        CheckActor(slot, attack.ActorObjectId);
        const auto first = slot * 100000U;
        if (attack.ShotSequence < first || attack.ShotSequence >= first + 100000U)
        { throw std::runtime_error("Melee damage key escapes the authenticated room slot."); }
        auto& member = members_.at(slot);
        if (Ended() || phase_ != RoomGamePhase::Playing || member.ActorPhase != RoomActorPhase::Alive)
        { return {}; }
        const auto* weapon = Battle::WeaponCatalog::Find(attack.WeaponId);
        // The ordinary equipped melee family is proven by the current
        // resource and 8ECB00/6F2700. Other forms and modifier lists need
        // their own authority; a client cannot supply a different loadout.
        if (!weapon || weapon->MasterId != 7000 ||
            member.Weapons[2] != attack.WeaponId || !attack.Values.empty()) { return {}; }
        if (attack.ShotSequence < member.NextDamagerKey) { return {}; }
        std::erase_if(member.Damagers, [&](const auto& entry)
            { return lastObserved_ - entry.second.Created > DamageResultLifetime; });
        if (member.Damagers.size() >= 4096) { throw std::runtime_error("Too many active room damage objects."); }
        const auto source = member.Movement.Latest(lastObserved_, std::chrono::seconds{5});
        std::array<std::uint64_t, 16> epochs{};
        for (const auto& [targetSlot, targetMember] : members_) { epochs[targetSlot] = targetMember.MovementEpoch; }
        // 6F2700 supplies the wire sequence through 7A1270 -> 795BB0
        // to GoDamager+692, shared with 8215 impact/808B removal. Coordinates are
        // the last reported actor position, not new client authority.
        member.Damagers.emplace(attack.ShotSequence, Damager{attack.WeaponId, 0,
            source.value_or(Battle::CombatVector{}), lastObserved_, source, epochs, DamagerKind::Melee});
        member.NextDamagerKey = attack.ShotSequence + 1;
        // +432 invokes the original attack animation on both local and
        // remote actors; the client also constructs its native melee object.
        return {{packet, {}, {}}};
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessWeaponImpact(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        const auto impact = Protocol::GameWeaponImpactNotification::Decode(packet);
        CheckActor(slot, impact.SourceObjectId);
        if (impact.DamagerKey < slot * 100000U || impact.DamagerKey >= (slot + 1U) * 100000U)
        { throw std::runtime_error("Weapon impact key escapes the authenticated room slot."); }
        for (const auto bits : impact.PositionBits)
        { if (!std::isfinite(std::bit_cast<float>(bits))) { throw std::runtime_error("Non-finite weapon impact."); } }
        if (Ended() || phase_ != RoomGamePhase::Playing) { return {}; }
        auto& member = members_.at(slot);
        const auto found = member.Damagers.find(impact.DamagerKey);
        if (found == member.Damagers.end()) { return {}; }
        auto& damager = found->second;
        if ((damager.Kind != DamagerKind::Melee && damager.Kind != DamagerKind::Scatter) ||
            lastObserved_ - damager.Created > DamageResultLifetime) { return {}; }
        std::optional<std::uint32_t> target;
        for (const auto& [candidate, state] : members_)
        { (void)state; if (ActorId(candidate) == impact.TargetObjectId) { target = candidate; break; } }
        if (!target) { return {}; }
        auto& victim = members_.at(*target);
        const auto bit = static_cast<std::uint16_t>(1U << *target);
        if ((damager.HitSlots & bit) || damager.TargetMovementEpochs[*target] != victim.MovementEpoch) { return {}; }
        const Battle::CombatVector point{std::bit_cast<float>(impact.PositionBits[0]),
            std::bit_cast<float>(impact.PositionBits[1]), std::bit_cast<float>(impact.PositionBits[2])};
        if (scene_)
        {
            const auto position = victim.Movement.Latest(lastObserved_, std::chrono::seconds{5});
            // Same local report envelope as ordinary shots; not native bones or authoritative HP.
            constexpr double envelope = 64.0 * 64.0;
            if (!position || !damager.ReportedSourcePosition ||
                Battle::CombatGeometry::DistanceSquared(point, *position) > envelope ||
                Battle::CombatGeometry::DistanceSquared(damager.Origin, *damager.ReportedSourcePosition) > envelope)
            { return {}; }
            const auto obstruction = scene_->FirstObstruction(damager.Origin, point, Battle::CombatSceneQuery::Bullet);
            const auto distance = std::sqrt(Battle::CombatGeometry::DistanceSquared(damager.Origin, point));
            if (obstruction && (1.0 - *obstruction) * distance > 2.0) { return {}; }
        }
        if (victim.ActorPhase == RoomActorPhase::Alive)
        { victim.DamageSources.insert_or_assign({ActorId(slot), damager.WeaponId}, lastObserved_ + DamageResultLifetime); }
        damager.HitSlots |= bit;
        // 6F4EF0 applies native melee/scatter damage. 808B independently retires the object.
        return {{packet, {}, {}}};
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessGrenadeDetonate(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        const auto data = Protocol::GameGrenadeDetonateNotification::Decode(packet);
        for (const auto key : {data.SourceDamagerKey, data.DamagerKey})
        {
            if (key < slot * 100000U || key >= (slot + 1U) * 100000U)
            { throw std::runtime_error("Grenade key escapes the authenticated room slot."); }
        }
        for (const auto& vector : {std::span<const std::uint32_t>(data.PositionBits), std::span<const std::uint32_t>(data.VectorBits)})
        { for (const auto bits : vector) { if (!std::isfinite(std::bit_cast<float>(bits))) { throw std::runtime_error("Non-finite grenade geometry."); } } }
        if (Ended() || phase_ != RoomGamePhase::Playing || !data.Values.empty()) { return {}; }
        auto& member = members_.at(slot);
        const auto source = member.Damagers.find(data.SourceDamagerKey);
        if (source == member.Damagers.end() || source->second.Kind != DamagerKind::Grenade ||
            data.DamagerKey < member.NextDamagerKey || lastObserved_ - source->second.Created > DamageResultLifetime)
        { return {}; }
        const Battle::CombatVector point{std::bit_cast<float>(data.PositionBits[0]),
            std::bit_cast<float>(data.PositionBits[1]), std::bit_cast<float>(data.PositionBits[2])};
        if (scene_)
        {
            const auto* weapon = Battle::CombatWeaponCatalog::Find(source->second.WeaponId);
            if (!weapon || !source->second.ReportedSourcePosition) { return {}; }
            // Weapon Speed is an engine input, not recovered world units/second.
            // Original 5033 travels about 305 units in three seconds with Speed=11.
            // Keep source consistency; trajectory/bounces remain client simulated.
            if (Battle::CombatGeometry::DistanceSquared(source->second.Origin, *source->second.ReportedSourcePosition) > 64.0 * 64.0)
            { return {}; }
        }
        const Damager scatter{source->second.WeaponId, 0, point, lastObserved_, point,
            source->second.TargetMovementEpochs, DamagerKind::Scatter};
        member.Damagers.emplace(data.DamagerKey, scatter);
        member.Damagers.erase(source);
        member.NextDamagerKey = data.DamagerKey + 1;
        // 6DF570 constructs GoScatter and destroys the parent. The thrower's
        // local consume performs the same operation; deliver once to its peers.
        return {{packet, {}, slot}};
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessStomp(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        const bool start = packet.MessageId == GameStompStartNotification::MessageId;
        const auto hit = start ? GameStompHitNotification{} : GameStompHitNotification::Decode(packet);
        const auto data = start ? GameStompStartNotification::Decode(packet) :
            GameStompStartNotification{hit.ActorObjectId, hit.TargetObjectId};
        CheckActor(slot, data.ActorObjectId);
        auto& member = members_.at(slot);
        if (Ended() || phase_ != RoomGamePhase::Playing || member.ActorPhase != RoomActorPhase::Alive) { return {}; }
        std::optional<std::uint32_t> target;
        for (const auto& [candidate, state] : members_)
        { (void)state; if (ActorId(candidate) == data.TargetObjectId) { target = candidate; break; } }
        if (!target || *target == slot) { return {}; }
        auto& victim = members_.at(*target);
        // Both 492E70 and 4A8BA0 require native state 401, established by
        // 8044 (respawn wait). A living bubble is state 301 and uses 8017.
        const bool eligible = victim.ActorPhase == RoomActorPhase::Waiting && member.Team && victim.Team && member.Team != victim.Team;
        if (start)
        {
            if (!eligible || (member.Stomp && member.Stomp->Expires > lastObserved_)) { return {}; }
            member.Stomp = Member::StompAction{*target, victim.MovementEpoch, lastObserved_ + std::chrono::seconds{5}, false};
        }
        else
        {
            if (!member.Stomp || member.Stomp->TargetSlot != *target || member.Stomp->Hit) { return {}; }
            if (!eligible || member.Stomp->TargetEpoch != victim.MovementEpoch || member.Stomp->Expires <= lastObserved_)
            {
                // Local cancellation policy: finish the actor, never hit a new life with an old action.
                member.Stomp.reset();
                return {{ClientToServerMessage0x802D{ActorId(slot)}.Encode(), {}, {}}};
            }
            member.Stomp->Hit = true;
        }
        // Native 8030 animates the attacker/target and consumes their stomp effects;
        // it does not prove a pang. Do not invent 9585, deaths, or score here.
        return {{packet, {}, {}}};
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessActorAction(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        std::uint32_t actor = 0;
        bool valid = true;
        switch (packet.MessageId)
        {
        case ClientToServerMessage0x800B::MessageId:
        {
            const auto data = ClientToServerMessage0x800B::Decode(packet);
            actor = data.ActorObjectId; valid = data.ValueAt4 <= 1; break;
        }
        case ClientToServerMessage0x800C::MessageId:
        {
            const auto data = ClientToServerMessage0x800C::Decode(packet);
            actor = data.ActorObjectId;
            // 8ED930 combines the two signed input axes with actor basis
            // vectors. Preserve the separate DWORD, whose complete role is
            // not established; it grants no position, damage or HP authority.
            const auto axis = [](std::uint8_t value) { return value <= 1 || value == 255; };
            valid = axis(data.AxisAt8) && axis(data.AxisAt9); break;
        }
        case ClientToServerMessage0x800D::MessageId:
        {
            const auto data = ClientToServerMessage0x800D::Decode(packet);
            actor = data.ActorObjectId;
            valid = data.ValueAt8 <= 1 && (data.ValueBitsAt4 & 0x7F800000U) != 0x7F800000U; break;
        }
        case ClientToServerMessage0x800E::MessageId:
            actor = ClientToServerMessage0x800E::Decode(packet).ActorObjectId; break;
        case ClientToServerMessage0x802D::MessageId:
            actor = ClientToServerMessage0x802D::Decode(packet).ActorObjectId; break;
        default: throw std::invalid_argument("Unsupported actor action.");
        }
        CheckActor(slot, actor);
        if (!valid) { throw std::runtime_error("Invalid ordinary actor action fields."); }
        if (Ended() || phase_ != RoomGamePhase::Playing || members_.at(slot).ActorPhase != RoomActorPhase::Alive)
        { return {}; }
        if (packet.MessageId == ClientToServerMessage0x802D::MessageId) { members_.at(slot).Stomp.reset(); }
        // Native receivers also clear the owner's pending action flags. The
        // client remains responsible for animation and movement integration.
        return {{packet, {}, {}}};
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessBubbleAction(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        const bool start = packet.MessageId == GameBubbleActionStartNotification::MessageId;
        const bool complete = packet.MessageId == GameBubbleActionCompleteNotification::MessageId;
        const auto action = start ? GameBubbleActionStartNotification::Decode(packet) :
            GameBubbleActionStartNotification{complete ? GameBubbleActionCompleteNotification::Decode(packet).ActorObjectId :
                GameBubbleActionEndNotification::Decode(packet).ActorObjectId, 0, 0};
        CheckActor(slot, action.ActorObjectId);
        auto& member = members_.at(slot);
        if (Ended() || phase_ != RoomGamePhase::Playing || member.ActorPhase != RoomActorPhase::Alive) { return {}; }
        if (!start)
        {
            if (!member.Action) { return {}; }
            if (!complete && member.Action->Ended) { return {}; }
            std::vector<RoomGameDelivery> output{{packet, {}, {}}};
            if (complete) { member.Action.reset(); }
            else { member.Action->Ended = true; }
            return output;
        }
        // Ordinary 50AEA0 and 8F2500 producers. NPC/item variants require
        // their own authority; do not turn them into a room actor.
        if (action.Variant != 1 && action.Variant != 2) { return {}; }
        std::optional<std::uint32_t> target;
        for (const auto& [candidate, value] : members_)
        {
            if (ActorId(candidate) == action.TargetObjectId && value.ActorPhase == RoomActorPhase::Bubble &&
                member.Team != 0 && value.Team != 0 && value.Team != member.Team) { target = candidate; break; }
        }
        if (!target) { return {}; }
        const auto epoch = members_.at(*target).MovementEpoch;
        if (member.Action && !member.Action->Ended && member.Action->Expires > lastObserved_ && member.Action->TargetSlot == *target &&
            member.Action->Variant == action.Variant && member.Action->TargetEpochs[*target] == epoch) { return {}; }
        Member::BubbleAction pending{*target, action.Variant, lastObserved_ + std::chrono::seconds{5}};
        for (const auto& [candidate, value] : members_)
        {
            if (value.ActorPhase == RoomActorPhase::Bubble && value.Team != 0 && value.Team != member.Team)
            {
                pending.EligibleTargets |= static_cast<std::uint16_t>(1U << candidate);
                pending.TargetEpochs[candidate] = value.MovementEpoch;
            }
        }
        // Owner also needs the echo: +508 clears its pending flag and enters
        // state 110. Distance/animation collision is still client-reported.
        std::vector<RoomGameDelivery> output{{packet, {}, {}}};
        member.Action = pending;
        return output;
    }

    std::vector<RoomGameDelivery> RoomGameSession::ProcessBubble(std::uint32_t slot, const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        const auto id = packet.MessageId;
        std::uint32_t actor = 0, source = 0, contact = 0, reason = 0;
        std::uint8_t kind = 0;
        const auto finite = [](std::uint32_t bits)
        {
            if ((bits & 0x7F800000U) == 0x7F800000U)
            { throw std::runtime_error("Non-finite bubble result value."); }
        };
        if (id == GameBubbleExitNotification::MessageId)
        {
            const auto data = GameBubbleExitNotification::Decode(packet);
            actor = data.ActorObjectId; source = data.SourceObjectId;
            CheckActor(slot, actor);
            // 510300 and 85C4C0 use these two sentinel sources.
            if (source != 0xFFFFFFFFU && source != 0xFFFFFFFEU)
            { throw std::runtime_error("Unsupported bubble exit source."); }
        }
        else if (id == GameBubbleShotPopRequest::MessageId)
        {
            const auto data = GameBubbleShotPopRequest::Decode(packet);
            actor = data.ActorObjectId; source = data.SourceObjectId;
            // Normal action 110 (8F0EF0) is sent by the source. The older
            // victim-owned 485BE0/weapon-5300 path remains unsupported.
            if (source != ActorId(slot))
            {
                CheckActor(slot, actor);
                return {};
            }
            CheckActor(slot, source);
        }
        else if (id == GameBubbleContactPopRequest::MessageId)
        {
            const auto data = GameBubbleContactPopRequest::Decode(packet);
            actor = data.ActorObjectId; source = data.SourceObjectId; contact = 1;
            CheckActor(slot, source);
        }
        else if (id == GameBubbleDeflectNotification::MessageId)
        {
            const auto data = GameBubbleDeflectNotification::Decode(packet);
            actor = data.ActorObjectId; source = data.SourceObjectId; contact = data.ContactValue;
            for (auto bits : data.VectorBits) { finite(bits); }
            if (data.ValueAt20 != 0xFFFFFFFFU || contact > 1)
            { throw std::runtime_error("Unsupported ordinary-room bubble deflection."); }
            CheckActor(slot, source);
        }
        else if (id == GameBubblePopNotification::MessageId)
        {
            const auto data = GameBubblePopNotification::Decode(packet);
            actor = data.ActorObjectId; source = data.SourceObjectId;
            contact = data.ContactValue; reason = data.ReasonValue;
            for (auto bits : data.VectorBits) { finite(bits); }
            if (data.ApplyValue != 1 || contact > 1 || (reason != 0 && reason != 2))
            { throw std::runtime_error("Unsupported ordinary-room bubble pop."); }
            CheckActor(slot, source);
        }
        else if (id == GameBubbleRescueRequest::MessageId)
        {
            const auto data = GameBubbleRescueRequest::Decode(packet);
            actor = data.ActorObjectId; source = data.HelperObjectId; kind = data.Kind;
            // 50A4D0 contact: helper reports; 485BE0 gun hit: victim reports.
            if (kind != 0 && kind != 1 && kind != 3)
            { throw std::runtime_error("Unsupported ordinary-room rescue kind."); }
            CheckActor(slot, kind == 3 ? actor : source);
        }
        else if (id == GameBubbleRescueNotification::MessageId)
        {
            const auto data = GameBubbleRescueNotification::Decode(packet);
            actor = data.ActorObjectId; source = data.HelperObjectId; kind = data.Kind;
            finite(data.ValueAt8);
            CheckActor(slot, actor);
        }
        else
        {
            const auto data = GameBubbleRescueEffectNotification::Decode(packet);
            actor = data.ActorObjectId; source = data.HelperObjectId; kind = data.Kind;
            // 4AAE70 writes the helper first, but the local victim sends it.
            CheckActor(slot, actor);
        }
        const auto findSlot = [&](std::uint32_t object)
        {
            for (const auto& [candidate, member] : members_)
            {
                (void)member;
                if (ActorId(candidate) == object) { return candidate; }
            }
            throw std::runtime_error("Bubble actor is not a room participant.");
        };
        const auto targetSlot = findSlot(actor);
        const auto sourceSlot = id == GameBubbleExitNotification::MessageId ? targetSlot : findSlot(source);
        if (Ended()) { return {}; }
        if (phase_ != RoomGamePhase::Playing)
        { throw std::runtime_error("Bubble interaction before shared game start."); }
        // Gun rescue kind 3 still requires weapon-5300 authority. 957F also
        // has a normal action producer, independently verified in 8F0EF0.
        if (id == GameBubbleRescueRequest::MessageId && kind == 3) { return {}; }
        auto& target = members_.at(targetSlot);
        auto& helper = members_.at(sourceSlot);
        // Explicit local pending lifetime; not the original rescue timer.
        const auto expires = lastObserved_ + std::chrono::seconds{5};
        for (auto* pending : {&target.Pop, &target.Rescue, &target.RescueEffect})
        { if (*pending && (*pending)->Expires <= lastObserved_) { pending->reset(); } }
        const auto matches = [&](const std::optional<Member::Interaction>& pending, std::uint8_t value)
        { return pending && pending->SourceSlot == sourceSlot && pending->Kind == value; };
        const bool sameTeam = target.Team != 0 && helper.Team != 0 && target.Team == helper.Team;
        const bool enemies = target.Team != 0 && helper.Team != 0 && target.Team != helper.Team;
        const auto forward = [&]() { return std::vector<RoomGameDelivery>{{packet, {}, {}}}; };
        if (id == GameBubbleRescueEffectNotification::MessageId)
        {
            if (target.ActorPhase != RoomActorPhase::Alive || !sameTeam || !matches(target.RescueEffect, kind))
            { return {}; }
            auto deliveries = forward();
            target.RescueEffect.reset();
            return deliveries;
        }
        if (target.ActorPhase != RoomActorPhase::Bubble) { return {}; }
        if (id == GameBubbleExitNotification::MessageId)
        {
            auto deliveries = forward();
            ClearBubbleLinks(targetSlot);
            target.ActorPhase = RoomActorPhase::Alive;
            ResetMovement(target);
            target.DamageSources.clear();
            return deliveries;
        }
        if (id == GameBubbleRescueRequest::MessageId)
        {
            if (sourceSlot == targetSlot || !sameTeam || helper.ActorPhase != RoomActorPhase::Alive ||
                target.Pop || target.Rescue) { return {}; }
            auto deliveries = forward();
            target.Rescue = Member::Interaction{sourceSlot, kind, expires};
            return deliveries;
        }
        if (id == GameBubbleRescueNotification::MessageId)
        {
            if (!sameTeam || helper.ActorPhase != RoomActorPhase::Alive || !matches(target.Rescue, kind)) { return {}; }
            auto deliveries = forward();
            AppendScore(deliveries, ScoreEvent::Rescue, sourceSlot, targetSlot);
            ClearBubbleLinks(targetSlot);
            target.ActorPhase = RoomActorPhase::Alive;
            ResetMovement(target);
            target.DamageSources.clear();
            target.RescueEffect = Member::Interaction{sourceSlot, kind, expires};
            return deliveries;
        }
        if (id == GameBubbleContactPopRequest::MessageId || id == GameBubbleShotPopRequest::MessageId)
        {
            if (!enemies || target.Pop || target.Rescue ||
                helper.ActorPhase != RoomActorPhase::Alive) { return {}; }
            const auto mask = static_cast<std::uint16_t>(1U << targetSlot);
            if (id == GameBubbleShotPopRequest::MessageId &&
                (!helper.Action || helper.Action->Ended || helper.Action->Expires <= lastObserved_ ||
                !(helper.Action->EligibleTargets & mask) || (helper.Action->RequestedTargets & mask) ||
                helper.Action->TargetEpochs[targetSlot] != target.MovementEpoch)) { return {}; }
            auto deliveries = forward();
            target.Pop = Member::Interaction{sourceSlot, static_cast<std::uint8_t>(contact), expires};
            if (id == GameBubbleShotPopRequest::MessageId) { helper.Action->RequestedTargets |= mask; }
            return deliveries;
        }
        // 85C4C0 can report an own timed pop (contact=1, reason=2).
        // Other area/item producers require evidence beyond this ordinary path.
        const bool selfPop = id == GameBubblePopNotification::MessageId &&
            targetSlot == sourceSlot && contact == 1 && reason == 2;
        if (!selfPop && (!enemies || reason != 0 || !matches(target.Pop, static_cast<std::uint8_t>(contact))))
        { return {}; }
        auto deliveries = forward();
        if (id == GameBubbleDeflectNotification::MessageId)
        {
            target.Pop.reset(); // 6FB680 applies effects, leaving the bubble.
        }
        else
        {
            AppendScore(deliveries, selfPop ? ScoreEvent::Death : ScoreEvent::Pang,
                selfPop ? targetSlot : sourceSlot, targetSlot);
            ClearBubbleLinks(targetSlot);
            target.ActorPhase = RoomActorPhase::Down; // native 4AEA80 requests state 405.
            ResetMovement(target);
            target.RespawnCompleted = false;
            target.DamageSources.clear();
        }
        return deliveries;
    }

    bool RoomGameSession::Ended() const noexcept
    {
        return phase_ == RoomGamePhase::AwaitingReports || phase_ == RoomGamePhase::Result ||
            phase_ == RoomGamePhase::ResultPresentation ||
            phase_ == RoomGamePhase::Returning;
    }

    void RoomGameSession::AppendScore(std::vector<RoomGameDelivery>& deliveries, ScoreEvent event,
        std::uint32_t sourceSlot, std::uint32_t targetSlot)
    {
        auto& source = members_.at(sourceSlot);
        auto& target = members_.at(targetSlot);
        if (source.Team == 0 || target.Team == 0) { return; } // Requires trusted roster binding.
        auto first = source.Scores;
        auto second = target.Scores;
        auto team = teamScores_[source.Team];
        const auto increment = [](std::uint16_t& value)
        { if (value != (std::numeric_limits<std::uint16_t>::max)()) { ++value; } };
        const auto index = event == ScoreEvent::Bubble ? 0U : event == ScoreEvent::Pang ? 1U :
            event == ScoreEvent::Rescue ? 2U : 3U;
        constexpr std::array<std::uint8_t, 4> keys{0, 12, 5, 14};
        increment(first[index]);
        Protocol::GameScoreUpdateNotification update;
        update.FirstSlot = static_cast<std::uint8_t>(sourceSlot);
        update.FirstUpdates.push_back({keys[index], first[index]});
        if (event == ScoreEvent::Pang)
        {
            increment(second[3]);
            increment(team);
            update.GroupIndex = source.Team;
            update.GroupValue = team;
            update.SecondSlot = static_cast<std::uint8_t>(targetSlot);
            update.SecondUpdates.push_back({keys[3], second[3]});
        }
        // Encode/allocate before committing counters. A confirmed opponent pang
        // gives one team point; this is explicit local scoring, not an original
        // server reward formula. The uint16 wire ceiling saturates, never wraps.
        deliveries.push_back({update.Encode(), {}, {}});
        source.Scores = first;
        if (event == ScoreEvent::Pang) { target.Scores = second; teamScores_[source.Team] = team; }
    }

    void RoomGameSession::ClearInteractions() noexcept
    {
        for (auto& [slot, member] : members_)
        {
            (void)slot;
            member.LoadingData.reset();
            member.Loaded.reset();
            member.Damagers.clear();
            member.Movement.Clear();
            member.DamageSources.clear();
            member.Pop.reset();
            member.Action.reset();
            member.Stomp.reset();
            member.Rescue.reset();
            member.RescueEffect.reset();
        }
    }

    void RoomGameSession::RequireSettlement()
    {
        if (phase_ != RoomGamePhase::Loading) { throw std::logic_error("Late settlement requirement."); }
        settlementRequired_ = true;
    }

    void RoomGameSession::ConfirmSettlement(Time now)
    {
        if (phase_ != RoomGamePhase::Result || !settlementRequired_ || settlementConfirmed_)
        { throw std::logic_error("Unexpected settlement confirmation."); }
        Observe(now);
        settlementConfirmed_ = true;
        deadline_ = now + ResultDisplay;
    }

    std::array<std::uint16_t, 4> RoomGameSession::FinalScores(std::uint32_t slot) const
    {
        if (phase_ != RoomGamePhase::Result) { throw std::logic_error("Scores are not frozen."); }
        return members_.at(slot).Scores;
    }

    std::array<std::uint16_t, 2> RoomGameSession::FinalTeamScores() const
    {
        if (phase_ != RoomGamePhase::Result) { throw std::logic_error("Team scores are not frozen."); }
        return {teamScores_[1], teamScores_[2]};
    }

    void RoomGameSession::Finish() noexcept
    {
        phase_ = RoomGamePhase::Returning;
        ClearInteractions();
        for (auto& [slot, member] : members_)
        { (void)slot; member.EndingReport.reset(); member.Scores = {}; }
        teamScores_ = {};
    }

    std::vector<RoomGameDelivery> RoomGameSession::Poll(Time now)
    {
        Observe(now);
        if (now < deadline_ || phase_ == RoomGamePhase::Returning) { return {}; }
        if (phase_ == RoomGamePhase::Playing)
        {
            std::vector<RoomGameDelivery> output{{Protocol::GameOverNotification{
                Protocol::GameOverNotification::TimeOver, 0}.Encode(), {}, {}}};
            phase_ = RoomGamePhase::AwaitingReports;
            gameOverAt_ = now;
            deadline_ = now + ReportLimit;
            ClearInteractions();
            return output;
        }
        if (phase_ == RoomGamePhase::AwaitingReports)
        {
            // 9E5E70/428B90 initialize winner=-1; S985 carries our actual team
            // totals. 42A340 compares that first column for ordinary mode 1.
            // These zero ancillary fields are NOT an explicit draw/winner.
            std::vector<RoomGameDelivery> output{{Protocol::GameResultNotification{0, duration_, 0, 0}.Encode(), {}, {}}};
            phase_ = RoomGamePhase::Result;
            deadline_ = now + ResultDisplay;
            return output;
        }
        if (phase_ == RoomGamePhase::Result)
        {
            if (settlementRequired_ && !settlementConfirmed_) { return {}; }
            std::vector<RoomGameDelivery> output{{Protocol::GameResultPresentationNotification{0}.Encode(), {}, {}}};
            phase_ = RoomGamePhase::ResultPresentation;
            deadline_ = now + ResultPresentationDisplay;
            return output;
        }
        if (phase_ == RoomGamePhase::ResultPresentation)
        {
            // Deliver to the old game context before RoomRegistry replaces it
            // with S117. Reversing this order would clean the new room context.
            std::vector<RoomGameDelivery> output{{Protocol::GameReturnToRoomNotification{}.Encode(), {}, {}}};
            Finish();
            return output;
        }
        if (phase_ != RoomGamePhase::Countdown)
        {
            // Loading timeout or completed local result display returns to waiting.
            Finish();
            return {};
        }
        std::vector<RoomGameDelivery> output{{Protocol::GameStartNotification{duration_, seed_}.Encode(), {}, {}}};
        phase_ = RoomGamePhase::Playing;
        deadline_ = now + std::chrono::milliseconds{duration_};
        return output;
    }
}
