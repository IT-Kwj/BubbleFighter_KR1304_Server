#pragma once

#include "TcpPacketCodec.h"

#include <string>

namespace BubbleFighter::Server::Protocol
{
    // E061: native Stock record is 28 bytes; its explicit wire form is 26.
    struct ShopPriceRecord
    {
        std::uint32_t StockId = 0;
        std::uint32_t ItemId = 0;
        // Resource isOnSale is NOT mapped here by 0x00A14C20. E062:
        // 0x00E157D0 skips price choices with this flag cleared.
        bool FlagAt12 = false;
        std::uint16_t Type = 0;
        std::uint32_t Duration = 0;
        std::uint32_t ShowPrice = 0;
        std::uint32_t SellPrice = 0;
        bool operator==(const ShopPriceRecord&) const = default;
    };

    struct ShopItemCardRecord
    {
        std::uint32_t Identifier = 0;
        std::uint32_t Kind = 0;
        std::u16string Name;
        std::uint32_t ValueAt36 = 0; // E062: card display-order comparator
        std::uint32_t MainCategory = 0;
        std::uint32_t SubCategory = 0;
        bool FlagAt48 = false; // E062: card visibility gate in 0x0064E210
        std::uint64_t WideAt56 = 0;
        std::uint64_t WideAt64 = 0;
        ShopPriceRecord Price;
        std::vector<ShopPriceRecord> Prices;
        bool operator==(const ShopItemCardRecord&) const = default;
    };

    struct ShopSubCategoryRecord
    {
        std::uint32_t Identifier = 0;
        std::u16string Name;
        std::uint32_t ValueAt32 = 0; // subcategory display order
        std::uint32_t ValueAt36 = 0; // E062: 0=subcategory, 1=promotion, 2=main-category page
        bool operator==(const ShopSubCategoryRecord&) const = default;
    };

    struct ShopMainCategoryRecord
    {
        std::uint32_t Identifier = 0;
        std::u16string Name;
        // 0x00646600 orders categories by native +32.
        std::uint32_t DisplayOrder = 0;
        std::vector<ShopSubCategoryRecord> SubCategories;
        bool operator==(const ShopMainCategoryRecord&) const = default;
    };

    struct ShopEntryResponse
    {
        static constexpr std::uint16_t MessageId = 800;
        // Local resource limits, not original server constants.
        static constexpr std::size_t MaxCollectionEntries = 8192;
        static constexpr std::size_t MaxTotalCollectionEntries = 32768;
        // Full pinned catalog is about 1.1 MiB. Keep the larger allowance local
        // to S800; inbound TCP decoding retains its existing 1 MiB bound.
        static constexpr std::size_t MaxPayloadBytes = 2 * 1024 * 1024;
        // The first value is NOT established as a success/result code.
        std::uint32_t ValueAt0 = 0;
        bool RefreshCatalog = false;
        std::uint64_t CatalogRevision = 0;
        std::vector<ShopItemCardRecord> Items;
        std::uint64_t WideAt32 = 0;
        std::vector<ShopMainCategoryRecord> Categories;
        std::vector<std::uint32_t> ValuesAt56;
        std::uint32_t SelectedCategory = 0;

        // All fields are present even when RefreshCatalog is false.
        TcpPacket Encode() const;
        static ShopEntryResponse Decode(const TcpPacket& packet);
        bool operator==(const ShopEntryResponse&) const = default;
    };
}
