#include "FighterMaximumHealth.h"
#include "FighterHealthCatalogData.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Battle
{
    FighterHealthEquipment FighterMaximumHealth::Equipment(std::uint32_t characterType,
        std::span<const std::uint32_t> items, std::uint32_t channel)
    {
        if (characterType >= 1000) { throw std::invalid_argument("Invalid fighter character key."); }
        FighterHealthEquipment result;
        const auto apply = [&](std::uint32_t item)
        {
            for (const auto& effect : FighterHealthEffects)
            {
                if (effect.ItemId != item || (effect.Channel != 0 && effect.Channel != channel)) { continue; }
                if (effect.Kind == 0) { result.Amount += std::clamp(effect.Value, 0.0F, 100.0F); }
                else { result.Ratio += std::clamp(effect.Value, 0.0F, 0.99F); }
            }
        };
        apply(300000U + characterType);
        const std::set<std::uint32_t> unique(items.begin(), items.end());
        for (auto item : unique)
        { if (item < 300000U || item >= 301000U) { apply(item); } }
        return result;
    }

    float FighterMaximumHealth::Ordinary(float baseHp, const FighterHealthEquipment& equipment,
        float actorScale, float luaScale)
    {
        for (float value : {baseHp, equipment.Amount, equipment.Ratio, actorScale, luaScale})
        { if (!std::isfinite(value)) { throw std::invalid_argument("Non-finite fighter health input."); } }
        if (baseHp <= 0 || actorScale <= 0 || luaScale <= 0)
        { throw std::invalid_argument("Non-positive fighter health scale."); }
        float hp = baseHp * actorScale;
        // Native aggregate guards IGNORE an out-of-range sum. These are not
        // the individual-item clamps in A04850; preserve both stages.
        if (equipment.Amount >= 0 && equipment.Amount <= 100.01200103759766F) { hp += equipment.Amount; }
        if (equipment.Ratio >= 0 && equipment.Ratio <= 0.9900000095367432F) { hp *= 1.0F + equipment.Ratio; }
        hp *= luaScale;
        if (!std::isfinite(hp) || hp <= 0) { throw std::overflow_error("Invalid fighter maximum health."); }
        return hp;
    }
}
