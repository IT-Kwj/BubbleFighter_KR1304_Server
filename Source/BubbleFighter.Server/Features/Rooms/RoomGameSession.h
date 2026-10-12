#pragma once
#include "Protocol/TcpPacketCodec.h"
#include "Protocol/ClientToServerMessage0x0243.h"
#include "Protocol/GameLoadingCompleteRequest.h"
#include "Protocol/GameReportRequest.h"
#include "Features/Battle/CombatMovementTimeline.h"
#include "Features/Battle/CombatScene.h"
#include "Features/Battle/FighterAttributeCatalog.h"
#include "Features/Battle/FighterMaximumHealth.h"
#include <chrono>
#include <array>
#include <map>
#include <optional>
#include <span>
#include <set>

namespace BubbleFighter::Server::Features::Rooms
{
    enum class RoomGamePhase { Loading, Countdown, Playing, AwaitingReports, Result, ResultPresentation, Returning };
    enum class RoomActorPhase { Alive, Bubble, Down, Waiting, AwaitingCompletion };
    struct RoomGameDelivery
    {
        Protocol::TcpPacket Packet;
        std::optional<std::uint32_t> OnlySlot;
        std::optional<std::uint32_t> ExceptSlot;
    };

    // Owned under the registry lock. All members share the same deadlines and seed.
    class RoomGameSession
    {
    public:
        using Clock = std::chrono::steady_clock;
        using Time = Clock::time_point;
        static constexpr auto LoadLimit = std::chrono::seconds{90};
        // 69C710 displays floor(remaining / 1000): 4000 yields 3, 2, 1 after the first tick.
        static constexpr auto Countdown = std::chrono::milliseconds{4000};
        // Local bounded presentation/report policy, not recovered server timers.
        static constexpr auto ReportLimit = std::chrono::seconds{10};
        static constexpr auto GameOverDisplay = std::chrono::seconds{3};
        static constexpr auto ResultDisplay = std::chrono::seconds{3};
        // Native S615 starts a four-second transition. Allow ten further
        // seconds for the panel; the total return deadline is local policy.
        static constexpr auto ResultPresentationDisplay = std::chrono::seconds{14};
        // Local maximum delay for a victim's result after an accepted hit.
        // This is a correlation bound, not a native projectile lifetime.
        static constexpr auto DamageResultLifetime = std::chrono::seconds{5};
        RoomGameSession(std::span<const std::uint32_t> slots, Time now, std::uint32_t seed,
            std::uint32_t durationMilliseconds);
        RoomGamePhase Phase() const noexcept { return phase_; }
        std::vector<RoomGameDelivery> Process(std::uint32_t slot, const Protocol::TcpPacket& packet, Time now);
        std::vector<RoomGameDelivery> Poll(Time now);
        void Finish() noexcept;
        // Trusted S117 equipment snapshot, installed by RoomRegistry before
        // loading. This is never populated from a gameplay packet.
        void BindEquipment(std::uint32_t slot, std::span<const std::uint32_t> items);
        void BindTeam(std::uint32_t slot, std::uint8_t team);
        void BindMap(std::u16string_view trustedMapKey);
        bool HasStaticCollision() const noexcept { return scene_ != nullptr; }
        void BindCharacter(std::uint32_t slot, std::uint32_t trustedCharacterType);
        const std::array<std::optional<Battle::FighterBaseAttributes>, 3>& BaseAttributes(std::uint32_t slot) const;
        const std::optional<Battle::FighterBaseAttributes>& SelectedBaseAttributes(std::uint32_t slot) const;
        // Initial ordinary-mode maximum, including resource equipment effects.
        // Not a mutable HP ledger and not a promise that skills are implemented.
        std::optional<float> InitialMaximumHealth(std::uint32_t slot) const;
        void RequireSettlement();
        void ConfirmSettlement(Time now);
        std::array<std::uint16_t, 4> FinalScores(std::uint32_t slot) const;
        std::array<std::uint16_t, 2> FinalTeamScores() const;
        static bool Handles(std::uint16_t messageId) noexcept;
        static std::uint32_t ActorId(std::uint32_t slot);
    private:
        enum class DamagerKind { Bullet, Melee, Grenade, Scatter };
        struct Damager
        {
            std::uint32_t WeaponId = 0;
            std::uint16_t HitSlots = 0;
            Battle::CombatVector Origin{};
            Time Created{};
            std::optional<Battle::CombatVector> ReportedSourcePosition;
            std::array<std::uint64_t, 16> TargetMovementEpochs{};
            DamagerKind Kind = DamagerKind::Bullet;
        };
        struct Member
        {
            struct BubbleAction
            {
                std::uint32_t TargetSlot, Variant;
                Time Expires;
                // Native action 110 visits nearby enemies, not only the clicked
                // target. Each current bubble may respond once per action.
                std::array<std::uint64_t, 16> TargetEpochs{};
                std::uint16_t EligibleTargets = 0, RequestedTargets = 0;
                // 8018 ends the hit window, but 8045 may still finish the
                // actor animation. Keep that authorization until completion.
                bool Ended = false;
            };
            struct Interaction
            {
                std::uint32_t SourceSlot;
                std::uint8_t Kind;
                Time Expires;
            };
            std::uint8_t Team = 0;
            // S985 keys 0/12/5/14: bubble, pang, help and death. Counts derive
            // only from accepted room transitions, never C103 client counters.
            std::array<std::uint16_t, 4> Scores{};
            std::optional<Protocol::GameReportRequest> EndingReport;
            std::optional<Interaction> Pop, Rescue, RescueEffect;
            std::optional<BubbleAction> Action;
            struct StompAction
            {
                std::uint32_t TargetSlot;
                std::uint64_t TargetEpoch;
                Time Expires;
                bool Hit = false;
            };
            std::optional<StompAction> Stomp;
            std::optional<Protocol::ClientToServerMessage0x0243> LoadingData;
            std::optional<Protocol::GameLoadingCompleteRequest> Loaded;
            bool ActorReported = false;
            RoomActorPhase ActorPhase = RoomActorPhase::Alive;
            bool BubbleSettled = false;
            bool RespawnCompleted = false;
            // BFCLIENT02 10EA690/10EAB10/817E80 built-in empty-slot fallback.
            std::array<std::uint32_t, 6> Weapons{2006, 1005, 7001, 5033, 0, 0};
            std::uint32_t NextDamagerKey = 0;
            // One bit per already-hit player; no unbounded event history.
            std::map<std::uint32_t, Damager> Damagers;
            Battle::CombatMovementTimeline Movement;
            std::uint64_t MovementEpoch = 0;
            std::array<std::optional<Battle::FighterBaseAttributes>, 3> BaseAttributes;
            std::optional<std::uint32_t> CharacterType;
            std::vector<std::uint32_t> EquipmentItems;
            std::optional<float> InitialMaximumHp;
            // Bounded by the participants and their four trusted gun slots.
            // Correlates a victim report even after its projectile retires.
            // This is not an HP or physical-collision authority.
            std::map<std::pair<std::uint32_t, std::uint32_t>, Time> DamageSources;
        };
        void Observe(Time now);
        void CheckActor(std::uint32_t slot, std::uint32_t actor) const;
        std::vector<RoomGameDelivery> ProcessActor(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessHit(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessDamageResult(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessBubble(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessBubbleAction(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessActorAction(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessMeleeAttack(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessWeaponImpact(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessGrenadeDetonate(std::uint32_t slot, const Protocol::TcpPacket& packet);
        std::vector<RoomGameDelivery> ProcessStomp(std::uint32_t slot, const Protocol::TcpPacket& packet);
        void ClearBubbleLinks(std::uint32_t slot) noexcept;
        static void RefreshInitialMaximumHealth(Member& member);
        static void ResetMovement(Member& member) noexcept;
        enum class ScoreEvent { Bubble, Pang, Rescue, Death };
        void AppendScore(std::vector<RoomGameDelivery>& deliveries, ScoreEvent event,
            std::uint32_t sourceSlot, std::uint32_t targetSlot);
        bool Ended() const noexcept;
        void ClearInteractions() noexcept;
        std::map<std::uint32_t, Member> members_;
        RoomGamePhase phase_ = RoomGamePhase::Loading;
        Time lastObserved_, deadline_;
        Time gameOverAt_{};
        std::array<std::uint16_t, 3> teamScores_{};
        std::uint32_t seed_, duration_;
        bool settlementRequired_ = false, settlementConfirmed_ = false;
        const Battle::CombatScene* scene_ = nullptr;
    };
}
