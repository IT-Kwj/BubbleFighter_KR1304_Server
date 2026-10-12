#pragma once
#include "Protocol/GameLoadingCompleteRequest.h"
#include "Protocol/GameReportRequest.h"
#include "Protocol/GameActorDeathNotification.h"
#include "Protocol/GameActorRespawnWaitNotification.h"
#include "Protocol/GameRespawnRequest.h"
#include "Protocol/GameRespawnCompletionNotification.h"
#include "Protocol/GameMovementNotification.h"
#include "Protocol/GameShotNotification.h"
#include "Protocol/GameExtendedShotNotification.h"
#include "Protocol/ClientToServerMessage0x0243.h"
#include "Protocol/ClientToServerMessage0x025F.h"
#include "Protocol/ClientToServerMessage0x0260.h"
#include "Protocol/ClientToServerMessage0x07ED.h"
#include <chrono>
#include <optional>

namespace BubbleFighter::Server::Features::Practice
{
    enum class PracticePhase { Loading, Countdown, Playing, AwaitingReport, Finishing, Result };

    class PracticeSession
    {
    public:
        using Clock = std::chrono::steady_clock;
        using Time = Clock::time_point;
        // E081: SceneRoster::ComposePractice fixes the local participant to
        // slot 0. Native 0x006D2040 resets that slot's object allocation range
        // and takes its first ID for ChrLocalPlayer. This is not CharacterId.
        static constexpr std::uint32_t LocalPlayerObjectId = 1'000'000;
        // Explicit local practice policy, not reconstructed Nexon durations.
        static constexpr auto LoadLimit = std::chrono::seconds{90};
        static constexpr auto Countdown = std::chrono::milliseconds{3000};
        static constexpr auto PlayLimit = std::chrono::milliseconds{240000};
        static constexpr auto ReportLimit = std::chrono::seconds{10};
        static constexpr auto GameOverDisplay = std::chrono::seconds{3};
        static constexpr auto ResultDisplay = std::chrono::seconds{3};
        PracticeSession(Time now, std::uint32_t seed);
        static std::uint32_t CreateRandomSeed();
        std::vector<Protocol::TcpPacket> LoadingComplete(const Protocol::GameLoadingCompleteRequest& report, Time now);
        std::vector<Protocol::TcpPacket> Poll(Time now);
        void SubmitReport(const Protocol::GameReportRequest& report, Time now);
        void SubmitLoadingData(const Protocol::ClientToServerMessage0x0243& report, Time now);
        void SubmitLoadingActor(const Protocol::ClientToServerMessage0x025F& report, Time now);
        std::vector<Protocol::TcpPacket> ReportActorDeath(const Protocol::GameActorDeathNotification& report, Time now);
        std::vector<Protocol::TcpPacket> ReportRespawnWait(const Protocol::GameActorRespawnWaitNotification& report, Time now);
        std::vector<Protocol::TcpPacket> RequestRespawn(const Protocol::GameRespawnRequest& request, Time now);
        std::vector<Protocol::TcpPacket> CompleteRespawn(const Protocol::GameRespawnCompletionNotification& report, Time now);
        void SubmitMovement(const Protocol::GameMovementNotification& report, Time now);
        std::vector<Protocol::TcpPacket> SubmitShot(const Protocol::GameShotNotification& report, Time now);
        std::vector<Protocol::TcpPacket> SubmitShot(const Protocol::GameExtendedShotNotification& report, Time now);
        const std::optional<Protocol::GameMovementNotification>& LastReportedMovement() const noexcept;
        void SubmitActionReport(const Protocol::ClientToServerMessage0x0260& report, Time now);
        void SubmitActionReport(const Protocol::ClientToServerMessage0x07ED& report, Time now);
        const std::optional<Protocol::ClientToServerMessage0x0260>& LastActionReport0x0260() const noexcept;
        const std::optional<Protocol::ClientToServerMessage0x07ED>& LastActionReport0x07ED() const noexcept;
        bool Expired(Time now);
        PracticePhase Phase() const noexcept;

    private:
        void Observe(Time now);
        void CheckLoadingReportWindow(bool duplicate, Time now);
        void CheckActorWindow(std::uint32_t actorObjectId, Time now);
        bool CanRetainActionReport(Time now);
        void ClearActionReports() noexcept;
        bool CanForwardShot(const Protocol::CompressedShotRecord& shot, Time now);
        enum class ActorPhase { Alive, Down, Waiting, AwaitingCompletion };
        PracticePhase phase_ = PracticePhase::Loading;
        Time lastObserved_;
        Time deadline_;
        Time gameOverAt_{};
        Time startedAt_{};
        std::uint32_t seed_;
        std::optional<Protocol::GameLoadingCompleteRequest> report_;
        std::optional<Protocol::GameReportRequest> endingReport_;
        std::optional<Protocol::ClientToServerMessage0x0243> loadingData_;
        std::optional<Protocol::ClientToServerMessage0x025F> loadingActor_;
        ActorPhase actorPhase_ = ActorPhase::Alive;
        bool respawnCompleted_ = false;
        std::optional<Protocol::GameMovementNotification> lastReportedMovement_;
        std::optional<Protocol::ClientToServerMessage0x0260> lastActionReport0x0260_;
        std::optional<Protocol::ClientToServerMessage0x07ED> lastActionReport0x07ED_;
    };
}
