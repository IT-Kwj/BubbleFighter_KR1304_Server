#include "CombatWeaponCatalog.h"
#include "CombatWeaponCatalogData.h"
#include <algorithm>

namespace BubbleFighter::Server::Features::Battle
{
    std::span<const CombatWeaponDefinition> CombatWeaponCatalog::Entries() noexcept { return CombatWeaponCatalogData; }

    const CombatWeaponDefinition* CombatWeaponCatalog::Find(std::uint32_t weaponId) noexcept
    {
        const auto found = std::lower_bound(CombatWeaponCatalogData.begin(), CombatWeaponCatalogData.end(), weaponId,
            [](const CombatWeaponDefinition& entry, std::uint32_t id) { return entry.WeaponId < id; });
        return found != CombatWeaponCatalogData.end() && found->WeaponId == weaponId ? &*found : nullptr;
    }
}
