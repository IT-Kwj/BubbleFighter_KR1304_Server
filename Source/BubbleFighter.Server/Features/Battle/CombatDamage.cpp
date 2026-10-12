#include "CombatDamage.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Battle
{
    namespace
    {
        void RequireNonnegative(float value)
        {
            if (!std::isfinite(value) || value < 0)
            { throw std::invalid_argument("Invalid combat numeric input."); }
        }
    }

    float CombatDamage::Calculate(float pureDamage, const CombatDamageFactors& factors)
    {
        RequireNonnegative(pureDamage);
        if (!std::isfinite(factors.AttackAdjustment) || !std::isfinite(factors.DefenseAdjustment))
        { throw std::invalid_argument("Invalid combat adjustment."); }
        RequireNonnegative(factors.CriticalMultiplier);
        RequireNonnegative(factors.HitMultiplier);
        RequireNonnegative(factors.RangeMultiplier);
        // Keep native float32 stage boundaries and operation order.
        float value = pureDamage * std::max(0.0f, 1.0f + factors.AttackAdjustment);
        value *= 1.0f + std::clamp(factors.DefenseAdjustment, -1.0f, 0.0f);
        value *= factors.CriticalMultiplier;
        value *= factors.HitMultiplier;
        value *= factors.RangeMultiplier;
        RequireNonnegative(value);
        return value;
    }

    CombatHealthChange CombatDamage::Apply(float currentHealth, float damage)
    {
        RequireNonnegative(currentHealth);
        RequireNonnegative(damage);
        const float remainder = currentHealth - damage;
        return {currentHealth, std::max(0.0f, remainder), damage,
            std::max(0.0f, -remainder), remainder <= 0};
    }
}
