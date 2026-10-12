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
        // Historical resource sale dates retained as UTC metadata. The full
        // local shop does not filter by them. These are NOT opaque wire fields.
        std::uint64_t AvailableFrom;
        std::uint64_t AvailableUntil;
        // E063 resource maxDuration; count/day unit follows ItemDefinition::Usage.
        std::uint32_t MaxAmount;
        // Preserve the signed resource value; it is not a boolean.
        std::int32_t BuyAgain;
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
