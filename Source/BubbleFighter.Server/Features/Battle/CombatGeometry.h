#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace BubbleFighter::Server::Features::Battle
{
    using CombatVector = std::array<float, 3>;
    struct CombatSphere { CombatVector Center; float Radius; };
    struct CombatCapsule { CombatVector Start, End; float Radius; };
    struct CombatTriangle { CombatVector A, B, C; };

    // Mathematical primitives for trusted scene geometry. A reported position
    // is not made authoritative merely by passing it to these functions.
    // Returned fractions are in [0, 1], relative to the complete segment.
    class CombatGeometry
    {
    public:
        static bool IsFinite(const CombatVector& point) noexcept;
        static CombatVector DecodeBinary16(const std::array<std::uint16_t, 3>& bits);
        static double DistanceSquared(const CombatVector& a, const CombatVector& b);
        static std::optional<double> IntersectSphere(const CombatVector& start,
            const CombatVector& end, const CombatSphere& sphere);
        // Continuous collision over one shared time interval; BFCD10 confirms
        // relative displacement and summed radii. Uses double intermediates.
        static std::optional<double> SweepSpheres(const CombatSphere& first,
            const CombatVector& firstEnd, const CombatSphere& second, const CombatVector& secondEnd);
        static std::optional<double> IntersectCapsule(const CombatVector& start,
            const CombatVector& end, const CombatCapsule& capsule);
        static std::optional<double> IntersectTriangle(const CombatVector& start,
            const CombatVector& end, const CombatTriangle& triangle);
        // Empty geometry is explicitly unsupported, never evidence of a clear
        // line of sight. The caller must bind a verified map before using it.
        static std::optional<double> FirstObstruction(const CombatVector& start,
            const CombatVector& end, std::span<const CombatTriangle> triangles);
    };
}
