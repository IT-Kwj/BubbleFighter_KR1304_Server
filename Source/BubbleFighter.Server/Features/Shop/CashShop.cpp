#include "CashShop.h"
#include "ShopCatalog.h"
#include "ShopStockCatalog.h"
#include "LocalShopCatalog.h"
#include "Features/Inventory/InventoryGrant.h"
#include "Features/Inventory/ItemCatalog.h"
#include "Features/Inventory/ItemUse.h"
#include "Features/Authentication/LocalMenuData.h"
#include "Features/Characters/CharacterProfile.h"
#include "Features/Characters/CharacterCreation.h"
#include "Protocol/InventoryViewUpdateNotification.h"
#include <algorithm>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Shop
{
    namespace
    {
        void Validate(const Characters::CharacterRecord& character, const Inventory::CharacterInventory& inventory,
            const Inventory::CashInventory& pending, std::uint64_t observedAt)
        {
            if (inventory.AccountId != character.AccountId || inventory.CharacterId != character.CharacterId ||
                pending.AccountId != character.AccountId || pending.CharacterId != character.CharacterId ||
                !character.Statistics.IsValid() || !Characters::CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex))
            { throw std::invalid_argument("Cash transaction ownership mismatch."); }
            (void)inventory.ToRecords(character.AccountId, observedAt);
            (void)pending.CreateNotification(character.AccountId).Encode();
            for (const auto& slot : inventory.Slots)
            {
                const auto* item = Inventory::ItemCatalog::Find(slot.ItemId);
                if (!item || item->Usage != slot.Usage) { throw std::invalid_argument("Cash transaction inventory catalog mismatch."); }
            }
        }
    }

    CashPurchaseResult CashShop::Purchase(const Protocol::ShopPurchaseRequest& request,
        const Characters::CharacterRecord& character, const Inventory::CharacterInventory& inventory,
        Inventory::CashInventory pending, std::uint32_t assignedSlotId, std::uint64_t observedAtUnixSeconds)
    {
        using namespace Inventory;
        using Response = Protocol::ShopPurchaseResponse;
        Validate(character, inventory, pending, observedAtUnixSeconds);
        CashPurchaseResult result;
        result.Purchase.Character = character;
        result.Purchase.Inventory = inventory;
        result.Purchase.Response.StockId = request.StockId;
        result.Pending = std::move(pending);
        const auto refuse = [&](std::uint32_t code)
        {
            result.Purchase.Response.Result = code;
            result.Purchase.Packets = {result.Purchase.Response.Encode()};
            return result;
        };
        const auto* stock = ShopStockCatalog::Find(request.StockId);
        if (!stock || request.ValueAt4 != 0) { return refuse(Response::Ineligible); }
        const auto* offer = ShopCatalog::FindItem(stock->ItemId);
        const auto* item = ItemCatalog::Find(stock->ItemId);
        // Original E17E80 treats currency kinds 1 (Cash) and 3 (CashCn)
        // against the same cash balance; neither uses the Lucci balance.
        if (!offer || !item || (offer->Currency != ShopCurrency::Cash && offer->Currency != ShopCurrency::CashCn) ||
            (item->Usage != InventoryUsage::Count && item->Usage != InventoryUsage::Duration && item->Usage != InventoryUsage::Life) ||
            stock->SellPrice > Characters::CharacterStatistics::MaxValue || stock->Duration == 0 ||
            (item->Usage == InventoryUsage::Duration && stock->Duration != 0xFFFFFFFFU && stock->Duration > 10000) ||
            (item->Usage != InventoryUsage::Duration && stock->Duration > InventorySlot::MaxQuantity))
        { return refuse(Response::Ineligible); }
        const auto chunks = item->Usage == InventoryUsage::Duration ? 1U : (stock->Duration + 65534U) / 65535U;
        if (chunks > Protocol::CashInventoryUpdateNotification::MaxSlots - result.Pending.Slots.size())
        { return refuse(Response::Ineligible); }
        const auto maxAmount = item->Usage == InventoryUsage::Count ? (std::max)(offer->MaxAmount, stock->Duration) : offer->MaxAmount;
        // Reserve grant capacity against all pending purchases of this item.
        // This projection is discarded; purchase does not start the duration.
        auto projected = inventory;
        for (const auto& slot : result.Pending.Slots)
        {
            if (slot.ItemId == stock->ItemId && InventoryGrant::Apply(projected, slot.ItemId, slot.Amount,
                slot.MaxAmount, observedAtUnixSeconds) != InventoryGrantStatus::Granted)
            { return refuse(Response::Ineligible); }
        }
        const auto grant = InventoryGrant::Apply(projected, stock->ItemId, stock->Duration, maxAmount, observedAtUnixSeconds);
        if (grant == InventoryGrantStatus::AlreadyOwned) { return refuse(Response::AlreadyOwned); }
        if (grant != InventoryGrantStatus::Granted) { return refuse(Response::Ineligible); }
        (void)projected.ToRecords(character.AccountId, observedAtUnixSeconds);
        if (character.Statistics.Cash < stock->SellPrice) { return refuse(Response::InsufficientCash); }
        if (assignedSlotId == 0 || assignedSlotId > CashInventory::MaxSlotId || chunks - 1 > CashInventory::MaxSlotId - assignedSlotId ||
            std::any_of(result.Pending.Slots.begin(), result.Pending.Slots.end(),
                [&](const auto& slot) { return slot.SlotId >= assignedSlotId && slot.SlotId - assignedSlotId < chunks; }))
        { throw std::invalid_argument("Invalid trusted cash slot allocation."); }
        auto remaining = stock->Duration;
        for (std::uint32_t index = 0; index < chunks; ++index)
        {
            const auto amount = item->Usage == InventoryUsage::Duration ? remaining : (std::min)(remaining, 65535U);
            result.Pending.Slots.push_back({assignedSlotId + index, stock->StockId, stock->ItemId, item->Usage,
                amount, maxAmount, index == 0 ? stock->SellPrice : 0U, observedAtUnixSeconds});
            remaining -= amount;
        }
        result.CashCost = stock->SellPrice;
        result.Purchase.Character.Statistics.Cash -= result.CashCost;
        result.Purchase.Response.Result = Response::Success;
        result.Purchase.Response.ValueAt40 = assignedSlotId;
        const auto data = Authentication::LocalMenuData::Create(result.Purchase.Character, inventory, observedAtUnixSeconds);
        const auto profile = Characters::CharacterProfile::BindKnownFields(data.Profile, result.Purchase.Character, character.AccountId);
        // S810 must precede S816: immediate C829 and S835 need the pending ID.
        result.Purchase.Packets = {profile.Encode(), result.Pending.CreateNotification(character.AccountId).Encode(),
            result.Purchase.Response.Encode()};
        return result;
    }

    CashPickupResult CashShop::Pickup(const Protocol::CashInventoryPickupRequest& request,
        const Characters::CharacterRecord& character, Inventory::CharacterInventory inventory,
        Inventory::CashInventory pending, std::uint64_t observedAtUnixSeconds)
    {
        using namespace Inventory;
        using Response = Protocol::CashInventoryPickupResponse;
        Validate(character, inventory, pending, observedAtUnixSeconds);
        CashPickupResult result{character, std::move(inventory), std::move(pending), {request.SlotId, Response::Failed, 0, {}}, 0, {}};
        const auto refuse = [&](std::uint32_t code)
        {
            result.Response.Result = code;
            result.Packets = {result.Response.Encode(), result.Pending.CreateNotification(character.AccountId).Encode()};
            return result;
        };
        const auto found = std::find_if(result.Pending.Slots.begin(), result.Pending.Slots.end(),
            [&](const auto& slot) { return slot.SlotId == request.SlotId; });
        if (found == result.Pending.Slots.end()) { return refuse(Response::Failed); }
        auto granted = result.Inventory;
        const auto status = InventoryGrant::Apply(granted, found->ItemId, found->Amount, found->MaxAmount, observedAtUnixSeconds);
        if (status == InventoryGrantStatus::AlreadyOwned) { return refuse(Response::AlreadyOwned); }
        if (status != InventoryGrantStatus::Granted) { return refuse(Response::Failed); }
        result.Inventory = std::move(granted);
        result.ChangedItemId = found->ItemId;
        result.Pending.Slots.erase(found);
        result.Response.Result = Response::Success;
        const auto data = Authentication::LocalMenuData::Create(character, result.Inventory, observedAtUnixSeconds);
        // S835 looks up and removes the previous cash slot. Send the refreshed
        // pending list AFTER it, or its result dialog cannot find the item.
        result.Packets = {result.Inventory.CreateNotification(character.AccountId, observedAtUnixSeconds).Encode(),
            Protocol::InventoryViewUpdateNotification{data.Equipment}.Encode(), result.Response.Encode(),
            result.Pending.CreateNotification(character.AccountId).Encode()};
        return result;
    }
}
