#include "PracticeSession.h"
#include "Protocol/ServerToClientMessage0x025E.h"
#include "Protocol/GameCountdownNotification.h"
#include "Protocol/GameStartNotification.h"
#include "Protocol/GameOverNotification.h"
#include "Protocol/GameResultNotification.h"
#include "Protocol/GameRespawnNotification.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <cmath>

namespace BubbleFighter::Server::Features::Practice
{
    PracticeSession::PracticeSession(Time now, std::uint32_t seed) :
        lastObserved_(now), deadline_(now + LoadLimit), seed_(seed)
    {
        // Avoid abs(INT_MIN) and degenerate boundary seeds in the client's RNG.
        if (seed == 0 || seed >= 0x7FFFFFFF) { throw std::invalid_argument("Invalid local practice random seed."); }
    }

    std::uint32_t PracticeSession::CreateRandomSeed()
    {
        for (;;)
        {
            std::uint32_t value = 0;
            if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&value), sizeof(value), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
            { throw std::runtime_error("Practice random seed generation failed."); }
            value &= 0x7FFFFFFF;
            if (value != 0 && value != 0x7FFFFFFF) { return value; }
        }
    }

    void PracticeSession::Observe(Time now)
    {
        if (now < lastObserved_) { throw std::runtime_error("Practice clock moved backwards."); }
        lastObserved_ = now;
    }

    std::vector<Protocol::TcpPacket> PracticeSession::LoadingComplete(
        const Protocol::GameLoadingCompleteRequest& report, Time now)
    {
        Observe(now);
        if (report.ValueAt8 != 0 || report.ValueAt12 != 0 ||
            (report.DelayMilliseconds != 0 && report.DelayMilliseconds != 3000))
        { throw std::runtime_error("Unsupported practice loading report."); }
        if (report_)
        {
            if (*report_ != report) { throw std::runtime_error("Changed duplicate practice loading report."); }
            return {}; // No clock, actor or seed reset in countdown or play.
        }
        if (phase_ != PracticePhase::Loading || now >= deadline_)
        { throw std::runtime_error("Late practice loading report."); }
        // S606 read/discard field uses the documented local zero baseline.
        std::vector<Protocol::TcpPacket> packets{Protocol::ServerToClientMessage0x025E{0}.Encode(),
            Protocol::GameCountdownNotification{static_cast<std::uint32_t>(Countdown.count())}.Encode()};
        report_ = report;
        phase_ = PracticePhase::Countdown;
        deadline_ = now + Countdown;
        return packets;
    }

    std::vector<Protocol::TcpPacket> PracticeSession::Poll(Time now)
    {
        Observe(now);
        if (now < deadline_) { return {}; }
        if (phase_ == PracticePhase::Finishing)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(gameOverAt_ - startedAt_).count();
            const auto bounded = std::min<std::int64_t>(elapsed, (std::numeric_limits<std::uint32_t>::max)());
            // E075: mode 40 has its own result image/telemetry path. The first
            // word is unused by this reader's consumers. Group result bytes
            // retain the native zero baseline for this unranked local practice;
            // neither comes from the 29 untrusted report counters. This is not
            // an original-server reward/result policy. See PracticeResultFlow.md.
            std::vector<Protocol::TcpPacket> packets{Protocol::GameResultNotification{
                0, static_cast<std::uint32_t>(bounded), 0, 0}.Encode()};
            phase_ = PracticePhase::Result;
            deadline_ = now + ResultDisplay;
            return packets;
        }
        if (phase_ == PracticePhase::Playing)
        {
            // E072: reason 2 is the native Time Over branch. In non-Zombie
            // mode 40 that branch does not use the second outcome argument;
            // zero is an explicit ignored-argument policy, not a winner code.
            std::vector<Protocol::TcpPacket> packets{Protocol::GameOverNotification{
                Protocol::GameOverNotification::TimeOver, 0}.Encode()};
            phase_ = PracticePhase::AwaitingReport;
            lastReportedMovement_.reset();
            ClearActionReports();
            gameOverAt_ = now;
            deadline_ = now + ReportLimit;
            return packets;
        }
        if (phase_ != PracticePhase::Countdown) { return {}; }
        std::vector<Protocol::TcpPacket> packets{Protocol::GameStartNotification{
            static_cast<std::uint32_t>(PlayLimit.count()), seed_}.Encode()};
        phase_ = PracticePhase::Playing;
        startedAt_ = now;
        deadline_ = now + PlayLimit; // Starts when S608 is prepared, never before it.
        return packets;
    }

    bool PracticeSession::Expired(Time now)
    {
        Observe(now);
        return (phase_ == PracticePhase::Loading || phase_ == PracticePhase::AwaitingReport ||
            phase_ == PracticePhase::Result) && now >= deadline_;
    }

    void PracticeSession::SubmitReport(const Protocol::GameReportRequest& report, Time now)
    {
        Observe(now);
        if (report.RandomSeed != seed_) { throw std::runtime_error("Practice report belongs to another start."); }
        if ((phase_ == PracticePhase::Finishing || phase_ == PracticePhase::Result) && endingReport_)
        {
            if (*endingReport_ != report) { throw std::runtime_error("Changed duplicate practice game report."); }
            return; // No delay extension, statistics, inventory or reward write.
        }
        if (phase_ != PracticePhase::AwaitingReport || now >= deadline_)
        { throw std::runtime_error("Practice report outside the server-initiated game-over window."); }
        // Public seed binds an already owned connection; it never authenticates
        // the client or validates the other 29 client-supplied counters.
        endingReport_ = report;
        phase_ = PracticePhase::Finishing;
        deadline_ = gameOverAt_ + GameOverDisplay;
    }

    std::vector<Protocol::TcpPacket> PracticeSession::ReportActorDeath(
        const Protocol::GameActorDeathNotification& report, Time now)
    {
        CheckActorWindow(report.ActorObjectId, now);
        if (actorPhase_ != ActorPhase::Alive) { return {}; }
        // Local unranked practice accepts the assigned player's own report;
        // it does not validate client physics or grant persistent rewards.
        std::vector<Protocol::TcpPacket> packets{report.Encode()};
        actorPhase_ = ActorPhase::Down;
        respawnCompleted_ = false;
        return packets;
    }

    void PracticeSession::CheckActorWindow(std::uint32_t actorObjectId, Time now)
    {
        Observe(now);
        if (actorObjectId != LocalPlayerObjectId || phase_ != PracticePhase::Playing || now >= deadline_)
        { throw std::runtime_error("Actor report outside the owned active practice scene."); }
    }

    std::vector<Protocol::TcpPacket> PracticeSession::ReportRespawnWait(
        const Protocol::GameActorRespawnWaitNotification& report, Time now)
    {
        CheckActorWindow(report.ActorObjectId, now);
        if (actorPhase_ == ActorPhase::Alive)
        { throw std::runtime_error("Respawn wait requires a current death report."); }
        if (actorPhase_ != ActorPhase::Down) { return {}; }
        std::vector<Protocol::TcpPacket> packets{report.Encode()};
        actorPhase_ = ActorPhase::Waiting;
        return packets;
    }

    std::vector<Protocol::TcpPacket> PracticeSession::RequestRespawn(
        const Protocol::GameRespawnRequest& request, Time now)
    {
        CheckActorWindow(request.Parameters.ActorObjectId, now);
        // Native mode-40 default sender omits both optional values. The
        // current GameOption.xml specifies itemResetOnRespawn=0. Alternate
        // spawn selection and revive items require separate evidence/policy.
        if (request.Parameters.ValueAt4 || request.Parameters.ValueAt12 || request.ResetItemsValue != 0)
        { throw std::runtime_error("Unsupported local practice respawn parameters."); }
        if (actorPhase_ == ActorPhase::AwaitingCompletion) { return {}; }
        if (actorPhase_ != ActorPhase::Waiting)
        { throw std::runtime_error("Respawn request outside the death/wait sequence."); }
        // Preserve the member +304 cleared baseline (0x009E1D70). E083
        // confirms zero passes the mode-40 predicate for either context flag.
        // This is an explicit local policy, not an inferred original life count.
        std::vector<Protocol::TcpPacket> packets{Protocol::GameRespawnNotification{
            request.Parameters, 0, 0}.Encode()};
        actorPhase_ = ActorPhase::AwaitingCompletion;
        lastReportedMovement_.reset(); // A respawn invalidates the old observed position.
        return packets;
    }

    std::vector<Protocol::TcpPacket> PracticeSession::CompleteRespawn(
        const Protocol::GameRespawnCompletionNotification& report, Time now)
    {
        CheckActorWindow(report.Parameters.ActorObjectId, now);
        if (report.Parameters.ValueAt4 || report.Parameters.ValueAt12 || report.ByteAt20 != 0 || report.ValueAt21 != 0)
        { throw std::runtime_error("Unsupported local practice respawn completion."); }
        if (actorPhase_ == ActorPhase::Alive && respawnCompleted_) { return {}; }
        if (actorPhase_ != ActorPhase::AwaitingCompletion)
        { throw std::runtime_error("Unsolicited practice respawn completion."); }
        // The native completion applies actor callbacks, including clearing
        // the fall/death suppression byte. Forward once before resetting the
        // server guard, so a later death can start a fresh cycle.
        std::vector<Protocol::TcpPacket> packets{report.Encode()};
        actorPhase_ = ActorPhase::Alive;
        respawnCompleted_ = true;
        return packets;
    }

    PracticePhase PracticeSession::Phase() const noexcept { return phase_; }

    bool PracticeSession::CanForwardShot(const Protocol::CompressedShotRecord& shot, Time now)
    {
        Observe(now);
        if (shot.ActorObjectId != LocalPlayerObjectId || shot.WeaponIndex >= 6 || !shot.HasFiniteCoordinates())
        { throw std::runtime_error("Invalid owned practice shot."); }
        // E086: current local-character sender routes to TCP; receiving the
        // shot creates the objects. Forward only within this unranked scene.
        // In-flight shots after death/game over cannot revive or prolong play.
        return phase_ == PracticePhase::Playing && now < deadline_ && actorPhase_ == ActorPhase::Alive;
    }

    std::vector<Protocol::TcpPacket> PracticeSession::SubmitShot(const Protocol::GameShotNotification& report, Time now)
    {
        if (!CanForwardShot(report.Shot, now)) { return {}; }
        return {report.Encode()};
    }

    std::vector<Protocol::TcpPacket> PracticeSession::SubmitShot(const Protocol::GameExtendedShotNotification& report, Time now)
    {
        // Encode first to validate sizes even for direct callers/late reports.
        auto packet = report.Encode();
        if (!CanForwardShot(report.Shot, now)) { return {}; }
        return {std::move(packet)};
    }

    bool PracticeSession::CanRetainActionReport(Time now)
    {
        Observe(now);
        // Local, unranked observation only. Early reports cannot start play;
        // in-flight reports after the deadline cannot extend the session.
        if ((phase_ != PracticePhase::Loading && phase_ != PracticePhase::Countdown &&
            phase_ != PracticePhase::Playing) || now >= deadline_)
        { ClearActionReports(); return false; }
        return true;
    }

    void PracticeSession::SubmitActionReport(const Protocol::ClientToServerMessage0x0260& report, Time now)
    {
        if (CanRetainActionReport(now)) { lastActionReport0x0260_ = report; }
    }

    void PracticeSession::SubmitActionReport(const Protocol::ClientToServerMessage0x07ED& report, Time now)
    {
        if (CanRetainActionReport(now)) { lastActionReport0x07ED_ = report; }
        // E085: event 41 precedes 0x9582 on the ordinary local fall path.
        // Receiving the report does not substitute for that death message or
        // permit direct C602. Mode 15's direct path is not mode 40.
    }

    const std::optional<Protocol::ClientToServerMessage0x0260>& PracticeSession::LastActionReport0x0260() const noexcept
    { return lastActionReport0x0260_; }

    const std::optional<Protocol::ClientToServerMessage0x07ED>& PracticeSession::LastActionReport0x07ED() const noexcept
    { return lastActionReport0x07ED_; }

    void PracticeSession::ClearActionReports() noexcept
    {
        lastActionReport0x0260_.reset();
        lastActionReport0x07ED_.reset();
    }

    void PracticeSession::SubmitMovement(const Protocol::GameMovementNotification& report, Time now)
    {
        Observe(now);
        if (report.ActorObjectId != LocalPlayerObjectId)
        { throw std::runtime_error("Movement does not belong to the assigned practice player."); }
        for (const auto value : report.Position)
        {
            if (!std::isfinite(value)) { throw std::runtime_error("Non-finite practice movement position."); }
        }
        // E084: the native timer predicate is false for an unstarted timer,
        // so movement alone cannot prove S608 has occurred. Retain one report
        // during loading/countdown/play; never infer object/death authority.
        // Late in-flight movement is discarded after game over or a deadline.
        if ((phase_ != PracticePhase::Loading && phase_ != PracticePhase::Countdown &&
            phase_ != PracticePhase::Playing) || now >= deadline_)
        { lastReportedMovement_.reset(); return; }
        lastReportedMovement_ = report;
        // No echo: 0x00703270 skips applying movement to a locally owned actor.
        // ValueAt20 is not used as a server clock/sequence or lifetime extension.
    }

    const std::optional<Protocol::GameMovementNotification>& PracticeSession::LastReportedMovement() const noexcept
    {
        return lastReportedMovement_;
    }

    void PracticeSession::CheckLoadingReportWindow(bool duplicate, Time now)
    {
        Observe(now);
        if ((phase_ != PracticePhase::Loading && (!duplicate ||
            (phase_ != PracticePhase::Countdown && phase_ != PracticePhase::Playing))) || now >= deadline_)
        { throw std::runtime_error("Practice loading data outside its local acceptance window."); }
    }

    void PracticeSession::SubmitLoadingData(const Protocol::ClientToServerMessage0x0243& report, Time now)
    {
        CheckLoadingReportWindow(loadingData_.has_value(), now);
        if (loadingData_)
        {
            if (*loadingData_ != report) { throw std::runtime_error("Changed duplicate practice loading data."); }
            return;
        }
        (void)report.Encode(); // Also bound callers that bypass wire decoding.
        loadingData_ = report;
        // Explicit unranked local policy: record only. Client map/spawn values
        // cannot create authoritative objects, rewards or extend any deadline.
    }

    void PracticeSession::SubmitLoadingActor(const Protocol::ClientToServerMessage0x025F& report, Time now)
    {
        if (report.ValueAt0 != LocalPlayerObjectId)
        { throw std::runtime_error("Loading actor does not match the assigned local practice slot."); }
        CheckLoadingReportWindow(loadingActor_.has_value(), now);
        if (loadingActor_ && *loadingActor_ != report)
        { throw std::runtime_error("Changed duplicate practice loading actor."); }
        // Confirm only the expected loading observation. This does not prove
        // client physics/death state or authorize a respawn or reward.
        loadingActor_ = report;
    }
}
