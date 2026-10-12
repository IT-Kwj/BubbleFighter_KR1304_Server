#include "ShopStockCatalog.h"
#include "ShopStockCatalogData.h"
#include "ShopFallbackStockData.h"

#include <algorithm>

namespace BubbleFighter::Server::Features::Shop
{
    std::span<const ShopStockDefinition> ShopStockCatalog::Entries() noexcept
    {
        return ShopStockCatalogData;
    }

    std::span<const ShopStockDefinition> ShopStockCatalog::FallbackEntries() noexcept
    { return ShopFallbackStockData; }

    const ShopStockDefinition* ShopStockCatalog::Find(std::uint32_t stockId) noexcept
    {
        const auto entries = Entries();
        const auto found = std::lower_bound(entries.begin(), entries.end(), stockId,
            [](const ShopStockDefinition& entry, std::uint32_t id) { return entry.StockId < id; });
        if (found != entries.end() && found->StockId == stockId) { return &*found; }
        const auto fallback = FallbackEntries();
        const auto local = std::lower_bound(fallback.begin(), fallback.end(), stockId,
            [](const ShopStockDefinition& entry, std::uint32_t id) { return entry.StockId < id; });
        return local != fallback.end() && local->StockId == stockId ? &*local : nullptr;
    }
}
