#include "ShopPurchase.h"
#include "LocalShopCatalog.h"
#include "ShopCatalog.h"
#include "ShopStockCatalog.h"
#include "Features/Authentication/LocalMenuData.h"
#include "Features/Characters/CharacterProfile.h"
#include "Features/Characters/CharacterCreation.h"
#include "Features/Inventory/ItemCatalog.h"
#include "Protocol/InventoryViewUpdateNotification.h"
#include "Features/Inventory/InventoryGrant.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Shop
{
    ShopPurchaseResult ShopPurchase::Prepare(const Protocol::ShopPurchaseRequest& request,
        const Characters::CharacterRecord& character, Inventory::CharacterInventory inventory,
        std::uint64_t observedAtUnixSeconds)
    {
        using namespace Inventory;
        using Response = Protocol::ShopPurchaseResponse;
        // Validate trusted state even for a refused offer. Corrupt state is a
        // server failure, not a successful purchase or an invented error code.
        if (inventory.AccountId != character.AccountId || inventory.CharacterId != character.CharacterId ||
            !character.Statistics.IsValid() || !Characters::CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex))
        { throw std::invalid_argument("Purchase inventory ownership is invalid."); }
        (void)inventory.ToRecords(character.AccountId, observedAtUnixSeconds);
        for (const auto& slot : inventory.Slots)
        {
            const auto* item = ItemCatalog::Find(slot.ItemId);
            if (!item || item->Usage != slot.Usage)
            { throw std::invalid_argument("Purchase inventory does not match the current item catalog."); }
        }
        // Render the equipment view after applying the grant: a just-expired
        // equipped target can become valid again through this renewal.
        ShopPurchaseResult result;
        result.Character = character;
        result.Inventory = std::move(inventory);
        result.Response.StockId = request.StockId;
        const auto refuse = [&](std::uint32_t status)
        {
            result.Response.Result = status;
            result.Packets = {result.Response.Encode()};
            return result;
        };
        const auto* stock = ShopStockCatalog::Find(request.StockId);
        if (request.ValueAt4 != 0 || !stock) { return refuse(Response::Ineligible); }
        const auto* offer = ShopCatalog::FindItem(stock->ItemId);
        const auto* definition = ItemCatalog::Find(stock->ItemId);
        // All resource and fallback Stock entries of a published item are offers.
        // Looking them up directly avoids rebuilding a 1 MiB catalog per buy.
        if (!offer || !definition || offer->Currency != ShopCurrency::Lucci ||
            (definition->Usage != InventoryUsage::Count && definition->Usage != InventoryUsage::Duration && definition->Usage != InventoryUsage::Life) ||
            stock->SellPrice > Characters::CharacterStatistics::MaxValue || stock->Duration == 0)
        { return refuse(Response::Ineligible); }

        auto granted = result.Inventory;
        const auto grant = InventoryGrant::Apply(granted, stock->ItemId, stock->Duration, offer->MaxAmount, observedAtUnixSeconds);
        if (grant == InventoryGrantStatus::AlreadyOwned) { return refuse(Response::AlreadyOwned); }
        if (grant != InventoryGrantStatus::Granted) { return refuse(Response::Ineligible); }
        if (character.Statistics.Lucci < stock->SellPrice) { return refuse(Response::InsufficientLucci); }
        result.Inventory = std::move(granted);
        result.Character.Statistics.Lucci -= stock->SellPrice;
        result.LucciCost = stock->SellPrice;
        result.ChangedItemId = stock->ItemId;
        result.Response.Result = Response::Success;
        // Ordinary Lucci UI: no special-shop selector or cash pickup identity.
        // Opaque zero/empty fields are an explicit local baseline, not claimed
        // original-server defaults. Updates precede the modal success dialog.
        const auto data = Authentication::LocalMenuData::Create(result.Character, result.Inventory, observedAtUnixSeconds);
        const auto profile = Characters::CharacterProfile::BindKnownFields(data.Profile, result.Character, character.AccountId);
        result.Packets = {profile.Encode(), result.Inventory.CreateNotification(character.AccountId, observedAtUnixSeconds).Encode(),
            Protocol::InventoryViewUpdateNotification{data.Equipment}.Encode(), result.Response.Encode()};
        return result;
    }
}
