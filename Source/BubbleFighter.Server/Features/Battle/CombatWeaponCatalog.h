#pragma once
#include <cstdint>
#include <span>

namespace BubbleFighter::Server::Features::Battle
{
    // BFCLIENT02 43B030: resolved base values, before equipment/mode/skill
    // modifiers. A catalog entry does not grant ownership or damage authority.
    struct CombatWeaponDefinition
    {
        std::uint32_t WeaponId, MasterId;
        float Speed, Range, HitRadius, Damage;
        std::int32_t FireCooldownMilliseconds;
        float NearRange, NearAngleDegrees;
        std::int32_t NearActiveMilliseconds, NearDelayMilliseconds;
        float ScatterSize1, ScatterSize2;
        std::int32_t ScatterDamage1, ScatterDamage2;
        float ScatterFirstDegree, ScatterSecondDegree;
        std::int32_t ScatterDelayMilliseconds;
    };

    class CombatWeaponCatalog
    {
    public:
        static std::span<const CombatWeaponDefinition> Entries() noexcept;
        static const CombatWeaponDefinition* Find(std::uint32_t weaponId) noexcept;
    };
}
