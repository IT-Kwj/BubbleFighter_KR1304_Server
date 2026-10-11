#include "ItemCatalog.h"
#include "ItemCatalogData.h"

#include <algorithm>

namespace BubbleFighter::Server::Features::Inventory
{
    std::span<const ItemDefinition> ItemCatalog::Entries() noexcept { return ItemCatalogData; }

    const ItemDefinition* ItemCatalog::Find(std::uint32_t itemId) noexcept
    {
        const auto found = std::lower_bound(ItemCatalogData.begin(), ItemCatalogData.end(), itemId,
            [](const ItemDefinition& definition, std::uint32_t id) { return definition.ItemId < id; });
        return found != ItemCatalogData.end() && found->ItemId == itemId ? &*found : nullptr;
    }
}
