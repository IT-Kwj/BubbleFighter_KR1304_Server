#include "WeaponCatalog.h"
#include "WeaponCatalogData.h"

#include <algorithm>

namespace BubbleFighter::Server::Features::Battle
{
    std::span<const WeaponDefinition> WeaponCatalog::Entries() noexcept { return WeaponCatalogData; }

    const WeaponDefinition* WeaponCatalog::Find(std::uint32_t weaponId) noexcept
    {
        const auto found = std::lower_bound(WeaponCatalogData.begin(), WeaponCatalogData.end(), weaponId,
            [](const WeaponDefinition& entry, std::uint32_t id) { return entry.WeaponId < id; });
        return found != WeaponCatalogData.end() && found->WeaponId == weaponId ? &*found : nullptr;
    }

    const WeaponDefinition* WeaponCatalog::FindEquipmentItem(std::uint32_t itemId) noexcept
    {
        return itemId >= 201000 && itemId <= 210999 ? Find(itemId - 200000) : nullptr;
    }
}
