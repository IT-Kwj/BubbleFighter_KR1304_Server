#include "CashInventory.h"
#include "ItemCatalog.h"
#include "ItemUse.h"
#include "Features/Shop/ShopStockCatalog.h"
#include "Features/Shop/ShopCatalog.h"
#include <set>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Inventory
{
    Protocol::CashInventoryUpdateNotification CashInventory::CreateNotification(std::uint64_t authenticatedAccountId) const
    {
        if (AccountId == 0 || AccountId != authenticatedAccountId || CharacterId == 0 || CharacterId > 0xFFFFFFFFULL ||
            Slots.size() > Protocol::CashInventoryUpdateNotification::MaxSlots)
        { throw std::invalid_argument("Invalid cash inventory owner or size."); }
        Protocol::CashInventoryUpdateNotification result;
        std::set<std::uint32_t> identifiers;
        for (const auto& slot : Slots)
        {
            const auto* stock = Shop::ShopStockCatalog::Find(slot.StockId);
            const auto* item = ItemCatalog::Find(slot.ItemId);
            const auto* offer = Shop::ShopCatalog::FindItem(slot.ItemId);
            if (slot.SlotId == 0 || slot.SlotId > MaxSlotId || !identifiers.insert(slot.SlotId).second ||
                !stock || !item || !offer || (offer->Currency != Shop::ShopCurrency::Cash && offer->Currency != Shop::ShopCurrency::CashCn) ||
                stock->ItemId != slot.ItemId || item->Usage != slot.Usage ||
                (stock->Duration != slot.Amount && !(slot.Usage != InventoryUsage::Duration && stock->Duration > 65535 && slot.Amount <= stock->Duration)) ||
                slot.MaxAmount == 0 ||
                slot.CashPrice > 0x7FFFFFFFU || slot.PurchasedAtUnixSeconds > InventorySlot::MaxUnixSeconds ||
                (slot.Usage != InventoryUsage::Count && slot.Usage != InventoryUsage::Duration && slot.Usage != InventoryUsage::Life) || slot.Amount == 0 ||
                (slot.Usage != InventoryUsage::Duration && slot.Amount > 65535) ||
                (slot.Usage == InventoryUsage::Duration && slot.Amount != 0xFFFFFFFFU && slot.Amount > 10000))
            { throw std::invalid_argument("Unsupported stored cash inventory slot."); }
            Protocol::CashInventorySlotRecord record;
            record.SlotId = slot.SlotId;
            record.StockId = slot.StockId;
            record.ItemId = slot.ItemId;
            if (slot.Usage != InventoryUsage::Duration) { record.Quantity = static_cast<std::uint16_t>(slot.Amount); }
            else { record.DurationDays = slot.Amount == 0xFFFFFFFFU ? 65535 : static_cast<std::uint16_t>(slot.Amount); }
            // No refund or external billing integration. Unresolved +8/text
            // fields use an explicit local zero/empty baseline.
            result.Slots.push_back(std::move(record));
        }
        return result;
    }
}
