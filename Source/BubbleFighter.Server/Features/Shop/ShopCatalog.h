#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace BubbleFighter::Server::Features::Shop
{
    enum class ShopCurrency { Cash, Lucci, CashCn };

    struct ShopItemDefinition
    {
        std::uint32_t ItemId;
        std::u16string_view Name;
        std::uint32_t MainCategory;
        std::uint32_t SubCategory;
        std::uint32_t DisplayOrder;
        ShopCurrency Currency;
        // Explicit local publication policy interprets unzoned XML dates as
        // UTC, with a half-open interval. These are NOT opaque wire fields.
        std::uint64_t AvailableFrom;
        std::uint64_t AvailableUntil;
    };

    struct ShopCategoryDefinition
    {
        std::uint32_t Identifier;
        std::u16string_view Name;
        std::uint32_t DisplayOrder;
        bool Visible;
    };

    struct ShopSubCategoryDefinition
    {
        std::uint32_t Identifier;
        std::uint32_t ParentCategory;
        std::u16string_view Name;
        std::uint32_t DisplayOrder;
        bool Visible;
        std::uint32_t PageType;
    };

    class ShopCatalog
    {
    public:
        static std::span<const ShopItemDefinition> Items() noexcept;
        static const ShopItemDefinition* FindItem(std::uint32_t itemId) noexcept;
        static std::span<const ShopCategoryDefinition> Categories() noexcept;
        static std::span<const ShopSubCategoryDefinition> SubCategories() noexcept;
    };
}
