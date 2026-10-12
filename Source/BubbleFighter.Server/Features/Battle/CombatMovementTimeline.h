#pragma once
#include "CombatGeometry.h"
#include <chrono>
#include <deque>

namespace BubbleFighter::Server::Features::Battle
{
    // Server receipt times only. The unidentified movement ValueAt20 and
    // client packed shot time cannot move this clock or authorize a rewind.
    // Samples remain reported positions until a movement solver validates them.
    class CombatMovementTimeline
    {
    public:
        using Time = std::chrono::steady_clock::time_point;
        struct Sample { Time Received; CombatVector Position; };
        static constexpr std::size_t MaxSamples = 128;
        static constexpr auto History = std::chrono::seconds{5};
        void Record(Time received, const CombatVector& position);
        void Clear() noexcept { samples_.clear(); }
        std::optional<CombatVector> Latest(Time now, std::chrono::milliseconds maxAge) const;
        // No interpolation across an unobserved long gap. 250 ms is a local
        // query bound, not a recovered movement rate or a physics guarantee.
        std::optional<CombatVector> At(Time when,
            std::chrono::milliseconds maxGap = std::chrono::milliseconds{250}) const;
        std::size_t Size() const noexcept { return samples_.size(); }
    private:
        std::deque<Sample> samples_;
    };
}
