#pragma once

#include <cstdint>
#include <span>

namespace BubbleFighter::Server::Features::Shop
{
    // Fingerprinted client Stock worksheet, not authoritative server offers.
    // Availability, sale dates, currencies and purchase rules need a separate
    // policy. ResourceListed does not map to ShopPriceRecord::FlagAt12.
    struct ShopStockDefinition
    {
        std::uint32_t StockId;
        std::uint32_t ItemId;
        bool ResourceListed;
        std::uint16_t Type;
        std::uint32_t Duration;
        std::uint32_t ShowPrice;
        std::uint32_t SellPrice;
        bool operator==(const ShopStockDefinition&) const = default;
    };

    class ShopStockCatalog
    {
    public:
        static std::span<const ShopStockDefinition> Entries() noexcept;
        // User-authorized local offers only for items without any resource Stock.
        static std::span<const ShopStockDefinition> FallbackEntries() noexcept;
        static const ShopStockDefinition* Find(std::uint32_t stockId) noexcept;
    };
}
