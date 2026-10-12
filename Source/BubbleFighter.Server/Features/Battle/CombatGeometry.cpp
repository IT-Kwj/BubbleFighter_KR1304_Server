#include "CombatGeometry.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Battle
{
    namespace
    {
        using Vector = std::array<double, 3>;
        Vector Subtract(const CombatVector& a, const CombatVector& b)
        { return {double(a[0]) - b[0], double(a[1]) - b[1], double(a[2]) - b[2]}; }
        double Dot(const Vector& a, const Vector& b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
        Vector Cross(const Vector& a, const Vector& b)
        { return {a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]}; }
        void Require(const CombatVector& point)
        {
            if (!CombatGeometry::IsFinite(point))
            { throw std::invalid_argument("Non-finite combat position."); }
        }
        void RequireRadius(float radius)
        {
            if (!std::isfinite(radius) || radius < 0)
            { throw std::invalid_argument("Invalid combat collision radius."); }
        }
        void KeepFirst(std::optional<double>& first, std::optional<double> candidate)
        { if (candidate && (!first || *candidate < *first)) { first = candidate; } }
        std::optional<double> SphereTime(const Vector& displacement, const Vector& direction, double radius)
        {
            const double c = Dot(displacement, displacement) - radius*radius;
            if (c <= 0) { return 0; }
            const double a = Dot(direction, direction), b = Dot(displacement, direction);
            if (a == 0 || b >= 0) { return {}; }
            const double determinant = b*b - a*c;
            if (determinant < 0) { return {}; }
            // Stable quadratic root (avoids cancellation for distant/small spheres).
            const double t = c / (-b + std::sqrt(determinant));
            return t >= 0 && t <= 1 ? std::optional{t} : std::nullopt;
        }
    }

    bool CombatGeometry::IsFinite(const CombatVector& point) noexcept
    { return std::all_of(point.begin(), point.end(), [](float x) { return std::isfinite(x); }); }

    CombatVector CombatGeometry::DecodeBinary16(const std::array<std::uint16_t, 3>& bits)
    {
        CombatVector point{};
        for (std::size_t i = 0; i < bits.size(); ++i)
        {
            const std::uint32_t exponent = (bits[i] >> 10) & 31U;
            const std::uint32_t mantissa = bits[i] & 1023U;
            if (exponent == 31) { throw std::invalid_argument("Non-finite compressed combat coordinate."); }
            float value = exponent == 0 ? std::ldexp(static_cast<float>(mantissa), -24) :
                std::ldexp(static_cast<float>(1024U + mantissa), static_cast<int>(exponent) - 25);
            if (bits[i] & 0x8000U) { value = -value; }
            point[i] = value;
        }
        return point;
    }

    double CombatGeometry::DistanceSquared(const CombatVector& a, const CombatVector& b)
    { Require(a); Require(b); const auto delta = Subtract(a, b); return Dot(delta, delta); }

    std::optional<double> CombatGeometry::IntersectSphere(const CombatVector& start,
        const CombatVector& end, const CombatSphere& sphere)
    {
        Require(start); Require(end); Require(sphere.Center); RequireRadius(sphere.Radius);
        return SphereTime(Subtract(start, sphere.Center), Subtract(end, start), sphere.Radius);
    }

    std::optional<double> CombatGeometry::SweepSpheres(const CombatSphere& first,
        const CombatVector& firstEnd, const CombatSphere& second, const CombatVector& secondEnd)
    {
        Require(first.Center); Require(firstEnd); Require(second.Center); Require(secondEnd);
        RequireRadius(first.Radius); RequireRadius(second.Radius);
        auto direction = Subtract(firstEnd, first.Center);
        const auto otherDirection = Subtract(secondEnd, second.Center);
        for (std::size_t i = 0; i < 3; ++i) { direction[i] -= otherDirection[i]; }
        return SphereTime(Subtract(first.Center, second.Center), direction, double(first.Radius) + second.Radius);
    }

    std::optional<double> CombatGeometry::IntersectCapsule(const CombatVector& start,
        const CombatVector& end, const CombatCapsule& capsule)
    {
        Require(start); Require(end); Require(capsule.Start); Require(capsule.End); RequireRadius(capsule.Radius);
        const auto axis = Subtract(capsule.End, capsule.Start), offset = Subtract(start, capsule.Start);
        const auto direction = Subtract(end, start);
        const double length = Dot(axis, axis), radius2 = double(capsule.Radius)*capsule.Radius;
        if (length == 0) { return IntersectSphere(start, end, {capsule.Start, capsule.Radius}); }
        const double oa = Dot(offset, axis), da = Dot(direction, axis);
        const double projected = std::clamp(oa / length, 0.0, 1.0);
        Vector closest{};
        for (std::size_t i = 0; i < 3; ++i) { closest[i] = offset[i] - projected * axis[i]; }
        if (Dot(closest, closest) <= radius2) { return 0; }
        std::optional<double> result = IntersectSphere(start, end, {capsule.Start, capsule.Radius});
        KeepFirst(result, IntersectSphere(start, end, {capsule.End, capsule.Radius}));
        // Remove the axis component before solving the finite cylinder.
        Vector radialDirection{}, radialOffset{};
        for (std::size_t i = 0; i < 3; ++i)
        {
            radialDirection[i] = direction[i] - axis[i] * da / length;
            radialOffset[i] = offset[i] - axis[i] * oa / length;
        }
        const double a = Dot(radialDirection, radialDirection), b = Dot(radialDirection, radialOffset);
        const double c = Dot(radialOffset, radialOffset) - radius2;
        const double determinant = b*b - a*c;
        if (a > 0 && determinant >= 0)
        {
            const double root = std::sqrt(determinant);
            for (double t : {(-b - root) / a, (-b + root) / a})
            {
                const double along = oa + t * da;
                if (t >= 0 && t <= 1 && along >= 0 && along <= length) { KeepFirst(result, t); }
            }
        }
        return result;
    }

    std::optional<double> CombatGeometry::IntersectTriangle(const CombatVector& start,
        const CombatVector& end, const CombatTriangle& triangle)
    {
        Require(start); Require(end); Require(triangle.A); Require(triangle.B); Require(triangle.C);
        const auto direction = Subtract(end, start), ab = Subtract(triangle.B, triangle.A);
        const auto ac = Subtract(triangle.C, triangle.A), fromA = Subtract(start, triangle.A);
        const auto normal = Cross(ab, ac);
        const double normal2 = Dot(normal, normal);
        if (normal2 == 0) { throw std::invalid_argument("Degenerate combat world triangle."); }
        const double denominator = Dot(normal, direction);
        const double numerator = -Dot(normal, fromA);
        if (denominator == 0)
        {
            if (numerator != 0) { return {}; }
            // In the plane, barycentric coordinates vary linearly along the
            // segment. Clip the interval against the three triangle edges.
            const double u0 = Dot(Cross(fromA, ac), normal) / normal2;
            const double v0 = Dot(Cross(ab, fromA), normal) / normal2;
            const double du = Dot(Cross(direction, ac), normal) / normal2;
            const double dv = Dot(Cross(ab, direction), normal) / normal2;
            double low = 0, high = 1;
            const auto clip = [&](double value, double change)
            {
                if (change == 0) { return value >= 0; }
                const double crossing = -value / change;
                if (change > 0) { low = std::max(low, crossing); }
                else { high = std::min(high, crossing); }
                return low <= high;
            };
            return clip(u0, du) && clip(v0, dv) && clip(1-u0-v0, -du-dv) ?
                std::optional{low} : std::nullopt;
        }
        const double t = numerator / denominator;
        if (t < 0 || t > 1) { return {}; }
        Vector point{};
        for (std::size_t i = 0; i < 3; ++i) { point[i] = fromA[i] + t * direction[i]; }
        const double u = Dot(Cross(point, ac), normal) / normal2;
        const double v = Dot(Cross(ab, point), normal) / normal2;
        constexpr double edgeTolerance = 1e-10;
        return u >= -edgeTolerance && v >= -edgeTolerance && u + v <= 1 + edgeTolerance ?
            std::optional{t} : std::nullopt;
    }

    std::optional<double> CombatGeometry::FirstObstruction(const CombatVector& start,
        const CombatVector& end, std::span<const CombatTriangle> triangles)
    {
        if (triangles.empty()) { throw std::invalid_argument("Combat map collision data is unavailable."); }
        std::optional<double> result;
        for (const auto& triangle : triangles) { KeepFirst(result, IntersectTriangle(start, end, triangle)); }
        return result;
    }
}
