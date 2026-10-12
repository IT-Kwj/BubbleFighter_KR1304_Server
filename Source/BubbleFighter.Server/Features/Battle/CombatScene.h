#pragma once
#include "CombatGeometry.h"
#include <span>
#include <string_view>
#include <vector>

namespace BubbleFighter::Server::Features::Battle
{
    enum class CombatSceneQuery : std::uint8_t { Bullet = 1, Grenade = 2, Actor = 4 };
    struct CombatSceneFace { CombatTriangle Triangle; std::uint8_t Queries; };

    // Trusted static resource subset. A missing obstruction is not evidence
    // that conditional/dynamic objects or actor sweeps have been simulated.
    class CombatScene
    {
    public:
        explicit CombatScene(std::span<const CombatSceneFace> faces);
        std::optional<double> FirstObstruction(const CombatVector& start,
            const CombatVector& end, CombatSceneQuery query) const;
        std::size_t FaceCount() const noexcept { return faces_.size(); }
        static const CombatScene* Find(std::u16string_view mapKey);
    private:
        struct Face
        {
            CombatSceneFace Source;
            CombatVector Minimum, Maximum;
        };
        std::vector<Face> faces_;
    };
}
