#include "ShopCatalog.h"
#include "ShopCatalogData.h"

#include <algorithm>

namespace BubbleFighter::Server::Features::Shop
{
    std::span<const ShopItemDefinition> ShopCatalog::Items() noexcept { return ShopCatalogItems; }
    const ShopItemDefinition* ShopCatalog::FindItem(std::uint32_t itemId) noexcept
    {
        const auto items = Items();
        const auto found = std::lower_bound(items.begin(), items.end(), itemId,
            [](const auto& item, std::uint32_t id) { return item.ItemId < id; });
        return found != items.end() && found->ItemId == itemId ? &*found : nullptr;
    }
    std::span<const ShopCategoryDefinition> ShopCatalog::Categories() noexcept { return ShopCatalogCategories; }
    std::span<const ShopSubCategoryDefinition> ShopCatalog::SubCategories() noexcept { return ShopCatalogSubCategories; }
}
