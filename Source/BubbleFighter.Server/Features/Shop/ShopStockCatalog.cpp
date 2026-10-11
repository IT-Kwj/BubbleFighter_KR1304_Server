#include "ShopStockCatalog.h"
#include "ShopStockCatalogData.h"

#include <algorithm>

namespace BubbleFighter::Server::Features::Shop
{
    std::span<const ShopStockDefinition> ShopStockCatalog::Entries() noexcept
    {
        return ShopStockCatalogData;
    }

    const ShopStockDefinition* ShopStockCatalog::Find(std::uint32_t stockId) noexcept
    {
        const auto entries = Entries();
        const auto found = std::lower_bound(entries.begin(), entries.end(), stockId,
            [](const ShopStockDefinition& entry, std::uint32_t id) { return entry.StockId < id; });
        return found != entries.end() && found->StockId == stockId ? &*found : nullptr;
    }
}
