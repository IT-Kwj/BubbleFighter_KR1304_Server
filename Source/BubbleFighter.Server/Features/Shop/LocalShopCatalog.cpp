#include "LocalShopCatalog.h"
#include "ShopCatalog.h"
#include "ShopStockCatalog.h"
#include "Features/Characters/CharacterCreation.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Shop
{
    namespace
    {
        std::array<std::uint8_t, 32> HashCatalog(std::span<const std::uint8_t> bytes)
        {
            if (bytes.size() > Protocol::ShopEntryResponse::MaxPayloadBytes)
            { throw std::invalid_argument("Catalog digest input exceeds the payload limit."); }
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            { throw std::runtime_error("Cannot initialize catalog digest."); }
            std::array<std::uint8_t, 32> digest{};
            const auto status = BCryptHash(algorithm, nullptr, 0, const_cast<PUCHAR>(bytes.data()),
                static_cast<ULONG>(bytes.size()), digest.data(), static_cast<ULONG>(digest.size()));
            BCryptCloseAlgorithmProvider(algorithm, 0);
            if (status < 0) { throw std::runtime_error("Cannot hash catalog snapshot."); }
            return digest;
        }
    }

    Protocol::ShopEntryResponse LocalShopCatalog::Create(const Protocol::ShopEntryRequest& request,
        const Characters::CharacterRecord& character, std::uint64_t observedAtUnixSeconds)
    {
        if (character.AccountId == 0 || character.CharacterId == 0 || !character.Statistics.IsValid() ||
            !Characters::CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex))
        { throw std::invalid_argument("Shop browsing requires a trusted persisted character."); }
        Protocol::ShopEntryResponse response;
        // 0x009ADA00 passes (ValueAt0 == 1) to 0x00646F80: the local catalog
        // suppresses the dynamic event-notice popup. This is not a success code.
        response.ValueAt0 = 1;
        response.RefreshCatalog = true;
        // Opaque wide fields and the exclusion list stay at the explicit local
        // zero/empty baseline. Their original server meanings remain unknown;
        // resource timestamps are used only for filtering, never reinterpreted
        // as these wire values. See E062 limitations before extending policy.
        std::map<std::uint32_t, std::size_t> itemIndices;
        for (const auto& definition : ShopCatalog::Items())
        {
            if (observedAtUnixSeconds < definition.AvailableFrom || observedAtUnixSeconds >= definition.AvailableUntil)
            { continue; }
            const auto categories = ShopCatalog::Categories();
            const auto category = std::find_if(categories.begin(), categories.end(),
                [&](const auto& value) { return value.Identifier == definition.MainCategory; });
            if (category == categories.end() || !category->Visible) { continue; }
            Protocol::ShopItemCardRecord card;
            card.Identifier = definition.ItemId;
            card.Kind = 0; // current ordinary item branch; StockSet bundles need their own data
            card.Name = definition.Name;
            card.ValueAt36 = definition.DisplayOrder;
            card.MainCategory = definition.MainCategory;
            card.SubCategory = definition.SubCategory;
            card.FlagAt48 = true; // current client's card visibility gate
            itemIndices.emplace(definition.ItemId, response.Items.size());
            response.Items.push_back(std::move(card));
        }
        // Keep the resource Stock-ID order, including all listed durations.
        // The first option is the local representative price, not a recovered
        // original server default. Client preview renders up to three options.
        for (const auto& stock : ShopStockCatalog::Entries())
        {
            if (!stock.ResourceListed) { continue; }
            const auto item = itemIndices.find(stock.ItemId);
            if (item == itemIndices.end()) { continue; }
            response.Items[item->second].Prices.push_back({stock.StockId, stock.ItemId, true,
                stock.Type, stock.Duration, stock.ShowPrice, stock.SellPrice});
        }
        for (auto& item : response.Items)
        {
            if (item.Prices.empty()) { throw std::runtime_error("Published item has no price options."); }
            item.Price = item.Prices.front();
        }
        for (const auto& definition : ShopCatalog::Categories())
        {
            if (!definition.Visible || std::none_of(response.Items.begin(), response.Items.end(),
                [&](const auto& item) { return item.MainCategory == definition.Identifier; }))
            { continue; }
            Protocol::ShopMainCategoryRecord category;
            category.Identifier = definition.Identifier;
            category.Name = definition.Name;
            category.DisplayOrder = definition.DisplayOrder;
            for (const auto& child : ShopCatalog::SubCategories())
            {
                if (child.ParentCategory == definition.Identifier && child.Visible)
                { category.SubCategories.push_back({child.Identifier, std::u16string(child.Name), child.DisplayOrder, child.PageType}); }
            }
            if (category.SubCategories.empty()) { throw std::runtime_error("Published category has no visible pages."); }
            response.Categories.push_back(std::move(category));
        }
        // Content revision excludes the requested selection and wall clock.
        // It changes when filtered offers, prices, labels or page data change.
        // SHA-256's first eight bytes are a cache hint, never authorization.
        const auto hash = HashCatalog(response.Encode().Payload);
        for (std::size_t index = 0; index < 8; ++index)
        { response.CatalogRevision |= static_cast<std::uint64_t>(hash[index]) << (8 * index); }
        if (response.CatalogRevision == 0) { response.CatalogRevision = 1; }
        if (!response.Categories.empty())
        {
            const auto requested = std::find_if(response.Categories.begin(), response.Categories.end(),
                [&](const auto& category) { return category.Identifier == request.SelectedCategory; });
            const auto fallback = std::min_element(response.Categories.begin(), response.Categories.end(),
                [](const auto& a, const auto& b) { return a.DisplayOrder < b.DisplayOrder; });
            response.SelectedCategory = (requested != response.Categories.end() ? requested : fallback)->Identifier;
        }
        return response;
    }
}
