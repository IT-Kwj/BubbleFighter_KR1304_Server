#pragma once

namespace BubbleFighter::Server::Features::Battle
{
    // Inputs must be derived from trusted equipment/mode/skill state. These
    // are aggregate modifiers, not fields accepted from a gameplay packet.
    struct CombatDamageFactors
    {
        float AttackAdjustment = 0;
        float DefenseAdjustment = 0;
        float CriticalMultiplier = 1;
        float HitMultiplier = 1;
        float RangeMultiplier = 1;
    };

    struct CombatHealthChange
    {
        float PreviousHealth, Health, AppliedDamage, ExcessDamage;
        bool Depleted;
    };

    class CombatDamage
    {
    public:
        // 6F3200: additive attack clamp, defense clamp, then critical.
        // 6F1460 applies the hit and distance multipliers afterwards.
        // Special intermediate reinforcement/shield operations must be resolved
        // by the caller; this arithmetic alone does not implement those effects.
        static float Calculate(float pureDamage, const CombatDamageFactors& factors);
        // 712E70: subtract HP, retain excess for the later state calculation,
        // and normalize depleted HP to zero. Does not guess Bubble vs Down.
        static CombatHealthChange Apply(float currentHealth, float damage);
    };
}
