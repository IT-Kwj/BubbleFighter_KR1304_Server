#include "CombatMovementTimeline.h"
#include <stdexcept>

namespace BubbleFighter::Server::Features::Battle
{
    void CombatMovementTimeline::Record(Time received, const CombatVector& position)
    {
        if (!CombatGeometry::IsFinite(position) || (!samples_.empty() && received < samples_.back().Received))
        { throw std::invalid_argument("Invalid combat movement sample."); }
        if (!samples_.empty() && received == samples_.back().Received)
        { samples_.back().Position = position; return; }
        samples_.push_back({received, position});
        while (samples_.size() > MaxSamples || samples_.front().Received < received - History)
        { samples_.pop_front(); }
    }

    std::optional<CombatVector> CombatMovementTimeline::Latest(Time now, std::chrono::milliseconds maxAge) const
    {
        if (maxAge.count() < 0) { throw std::invalid_argument("Negative movement age."); }
        if (samples_.empty() || now < samples_.back().Received || now - samples_.back().Received > maxAge)
        { return std::nullopt; }
        return samples_.back().Position;
    }

    std::optional<CombatVector> CombatMovementTimeline::At(Time when, std::chrono::milliseconds maxGap) const
    {
        if (maxGap.count() < 0) { throw std::invalid_argument("Negative movement interpolation gap."); }
        if (samples_.empty() || when < samples_.front().Received || when > samples_.back().Received)
        { return std::nullopt; }
        auto previous = samples_.begin();
        for (auto next = samples_.begin(); next != samples_.end(); ++next)
        {
            if (next->Received == when) { return next->Position; }
            if (next->Received > when)
            {
                if (next->Received - previous->Received > maxGap) { return std::nullopt; }
                const double fraction = std::chrono::duration<double>(when - previous->Received).count() /
                    std::chrono::duration<double>(next->Received - previous->Received).count();
                CombatVector value{};
                for (std::size_t i = 0; i < value.size(); ++i)
                { value[i] = static_cast<float>(double(previous->Position[i]) + fraction *
                    (double(next->Position[i]) - previous->Position[i])); }
                return value;
            }
            previous = next;
        }
        return std::nullopt;
    }
}
