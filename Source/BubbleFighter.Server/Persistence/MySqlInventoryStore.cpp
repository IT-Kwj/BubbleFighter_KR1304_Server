#include "MySqlInventoryStore.h"
#include "MySqlCommands.h"
#include "Features/Inventory/DefaultInventory.h"
#include "Features/Shop/ShopCatalog.h"
#include "Features/Shop/ShopStockCatalog.h"

#include <algorithm>
#include <array>
#include <limits>

namespace BubbleFighter::Server::Persistence
{
    using namespace Features::Inventory;
    using namespace MySqlCommands;

    namespace
    {
        bool LockOwner(MYSQL* connection, std::uint64_t accountId, std::uint64_t characterId)
        {
            if (accountId == 0 || characterId == 0 || characterId > 0xFFFFFFFFULL)
            { throw std::invalid_argument("Invalid inventory owner."); }
            // Every inventory writer takes the character lock before inspecting
            // rows, so concurrent additions cannot bypass the aggregate limits.
            Statement query(connection, "SELECT c.CharacterId FROM Characters c JOIN Accounts a ON a.AccountId = c.AccountId "
                "WHERE c.CharacterId = ? AND c.AccountId = ? AND a.IsDisabled = 0 FOR UPDATE");
            std::array parameters{Number(characterId), Number(accountId)};
            query.Execute(parameters);
            std::uint64_t id = 0;
            std::array results{Number(id)};
            return query.Fetch(results);
        }

        CharacterInventory LoadSlots(MYSQL* connection, std::uint64_t accountId, std::uint64_t characterId)
        {
            CharacterInventory inventory{accountId, characterId, {}};
            // One over the supported count detects an oversized database without
            // allocating an unbounded result. Details are versioned opaque wire
            // fields until their business semantics are established.
            Statement query(connection, "SELECT ItemId, UsageKind, Quantity, ExpiresAtUnixSeconds, IsEquipped, "
                "DetailVersion, DetailData, Revision FROM InventorySlots WHERE CharacterId = ? ORDER BY ItemId LIMIT 1025");
            std::array parameters{Number(characterId)};
            query.Execute(parameters);
            InventorySlot slot;
            std::uint8_t usage = 0, equipped = 0, version = 0;
            std::uint64_t expiry = 0;
            bool expiryNull = false;
            std::vector<std::uint8_t> detail(Protocol::InventoryItemRecord::MaxUncompressedBytes);
            unsigned long length = 0;
            std::array results{Number(slot.ItemId), Number(usage), Number(slot.Quantity), Number(expiry),
                Number(equipped), Number(version), Bytes(detail.data(), static_cast<unsigned long>(detail.size())), Number(slot.Revision)};
            results[3].is_null = &expiryNull;
            results[6].length = &length;
            std::size_t totalBytes = 4;
            std::size_t totalEntries = 0;
            while (query.Fetch(results))
            {
                if (inventory.Slots.size() == Protocol::InventoryItemRecord::MaxCollectionEntries ||
                    version != 1 || equipped > 1 || slot.Revision == 0 || length < 29 || length > detail.size() ||
                    length - 4 > Protocol::InventoryItemRecord::MaxUncompressedBytes - totalBytes)
                { throw std::runtime_error("Unsupported stored inventory slot."); }
                totalBytes += length - 4;
                const auto records = Protocol::InventoryItemRecord::DecodeList(std::span(detail).first(length));
                if (records.size() != 1 || records[0].Key != 0 || records[0].ValueAt4 != 0 || records[0].BitsAt8 != 0 || records[0].Flags != 0)
                { throw std::runtime_error("Invalid stored inventory detail envelope."); }
                std::size_t entries = 1 + records[0].AttributesAt24.size();
                for (const auto& attribute : records[0].AttributesAt24) { entries += attribute.ValuesAt8.size(); }
                if (entries > Protocol::InventoryItemRecord::MaxTotalCollectionEntries - totalEntries)
                { throw std::runtime_error("Stored inventory exceeds the aggregate entry limit."); }
                totalEntries += entries;
                slot.Usage = static_cast<InventoryUsage>(usage);
                slot.Equipped = equipped != 0;
                slot.ExpiresAtUnixSeconds = expiryNull ? std::nullopt : std::optional(expiry);
                slot.TextAt36 = records[0].TextAt36;
                slot.AttributesAt24 = records[0].AttributesAt24;
                inventory.Slots.push_back(slot);
            }
            return inventory;
        }
        WeaponPresetState LoadPresets(MYSQL* connection, const CharacterInventory& inventory)
        {
            WeaponPresetState state;
            std::array parameters{Number(inventory.CharacterId)};
            std::uint8_t count = 0;
            bool present = false;
            {
                // Statement uses an unbuffered result. Release the profile
                // cursor before issuing the second query on this connection.
                Statement query(connection, "SELECT ActiveIndex, PageCount FROM WeaponPresetState WHERE CharacterId = ?");
                query.Execute(parameters);
                std::array results{Number(state.ActiveIndex), Number(count)};
                present = query.Fetch(results);
            }
            if (present)
            {
                if (count == 0 || count > 5 || state.ActiveIndex >= count) { throw std::runtime_error("Invalid stored preset count."); }
                state.PrimaryItems.clear();
                Statement slots(connection, "SELECT PresetIndex, PrimaryItemId FROM WeaponPresetSlots WHERE CharacterId = ? ORDER BY PresetIndex LIMIT 6");
                slots.Execute(parameters);
                std::uint8_t index = 0;
                std::uint32_t item = 0;
                std::array slotResults{Number(index), Number(item)};
                while (slots.Fetch(slotResults))
                {
                    if (index != state.PrimaryItems.size() || state.PrimaryItems.size() == count)
                    { throw std::runtime_error("Invalid stored preset slots."); }
                    state.PrimaryItems.push_back(item);
                }
                if (state.PrimaryItems.size() != count) { throw std::runtime_error("Missing stored preset slot."); }
            }
            state.CaptureActive(inventory);
            return state;
        }

        void PersistPresets(MYSQL* connection, std::uint64_t characterId, const WeaponPresetState& state)
        {
            state.Validate();
            const auto count = static_cast<std::uint8_t>(state.PrimaryItems.size());
            Statement profile(connection, "INSERT INTO WeaponPresetState (CharacterId, ActiveIndex, PageCount) VALUES (?, ?, ?) "
                "ON DUPLICATE KEY UPDATE ActiveIndex = ?, PageCount = ?");
            std::array parameters{Number(characterId), Number(state.ActiveIndex), Number(count), Number(state.ActiveIndex), Number(count)};
            profile.Execute(parameters);
            Statement clear(connection, "DELETE FROM WeaponPresetSlots WHERE CharacterId = ?");
            std::array owner{Number(characterId)}; clear.Execute(owner);
            for (std::uint8_t index = 0; index != count; ++index)
            {
                Statement insert(connection, "INSERT INTO WeaponPresetSlots (CharacterId, PresetIndex, PrimaryItemId) VALUES (?, ?, ?)");
                std::array row{Number(characterId), Number(index), Number(state.PrimaryItems[index])}; insert.Execute(row);
                if (insert.AffectedRows() != 1) { throw std::runtime_error("Preset slot insert failed."); }
            }
        }

        void PersistEquipment(MYSQL* connection, const CharacterInventory& before, CharacterInventory& after)
        {
            if (before.Slots.size() != after.Slots.size()) { throw std::logic_error("Equipment change cannot grant items."); }
            for (std::size_t index = 0; index != before.Slots.size(); ++index)
            {
                const auto& old = before.Slots[index];
                auto& changed = after.Slots[index];
                if (old.ItemId != changed.ItemId) { throw std::logic_error("Equipment slot identity changed."); }
                if (old.Equipped == changed.Equipped) { continue; }
                if (old.Revision == (std::numeric_limits<std::uint64_t>::max)())
                { throw std::runtime_error("Equipment revision exhausted."); }
                const std::uint8_t equipped = changed.Equipped ? 1 : 0;
                const auto revision = old.Revision + 1;
                Statement update(connection, "UPDATE InventorySlots SET IsEquipped = ?, Revision = ? "
                    "WHERE CharacterId = ? AND ItemId = ? AND Revision = ?");
                std::array parameters{Number(equipped), Number(revision), Number(before.CharacterId), Number(old.ItemId), Number(old.Revision)};
                update.Execute(parameters);
                if (update.AffectedRows() != 1) { throw std::runtime_error("Equipment revision changed during transaction."); }
                changed.Revision = revision;
            }
        }

        void PersistGrant(MYSQL* connection, const Features::Characters::CharacterRecord& character, InventorySlot& slot)
        {
            const auto previousRevision = slot.Revision;
            const auto revision = previousRevision + 1; // preparation already checked overflow
            const auto usage = static_cast<std::uint8_t>(slot.Usage);
            const auto expiry = slot.ExpiresAtUnixSeconds.value_or(0);
            bool expiryNull = !slot.ExpiresAtUnixSeconds;
            if (previousRevision == 0)
            {
                Protocol::InventoryItemRecord detail;
                detail.TextAt36 = slot.TextAt36;
                detail.AttributesAt24 = slot.AttributesAt24;
                const auto bytes = Protocol::InventoryItemRecord::EncodeList(std::span(&detail, 1));
                const std::uint8_t equipped = slot.Equipped ? 1 : 0;
                Statement insert(connection, "INSERT INTO InventorySlots (CharacterId, ItemId, UsageKind, Quantity, ExpiresAtUnixSeconds, "
                    "IsEquipped, DetailVersion, DetailData, Revision) VALUES (?, ?, ?, ?, ?, ?, 1, ?, ?)");
                std::array parameters{Number(character.CharacterId), Number(slot.ItemId), Number(usage), Number(slot.Quantity),
                    Number(expiry), Number(equipped), Bytes(bytes.data(), static_cast<unsigned long>(bytes.size())), Number(revision)};
                parameters[4].is_null = &expiryNull;
                insert.Execute(parameters);
                if (insert.AffectedRows() != 1) { throw std::runtime_error("Purchase inventory insert failed."); }
            }
            else
            {
                // Preserve equipped state and opaque detail; buying never equips.
                Statement update(connection, "UPDATE InventorySlots SET Quantity = ?, ExpiresAtUnixSeconds = ?, Revision = ? "
                    "WHERE CharacterId = ? AND ItemId = ? AND Revision = ? AND UsageKind = ?");
                std::array parameters{Number(slot.Quantity), Number(expiry), Number(revision), Number(character.CharacterId),
                    Number(slot.ItemId), Number(previousRevision), Number(usage)};
                parameters[1].is_null = &expiryNull;
                update.Execute(parameters);
                if (update.AffectedRows() != 1) { throw std::runtime_error("Purchase inventory revision changed."); }
            }
            slot.Revision = revision;
        }

        CashInventory LoadCashSlots(MYSQL* connection, std::uint64_t accountId, std::uint64_t characterId)
        {
            CashInventory inventory{accountId, characterId, {}};
            Statement query(connection, "SELECT SlotId, StockId, ItemId, UsageKind, Amount, MaxAmount, CashPrice, PurchasedAtUnixSeconds "
                "FROM CashInventorySlots WHERE CharacterId = ? ORDER BY SlotId LIMIT 1025");
            std::array parameters{Number(characterId)}; query.Execute(parameters);
            CashInventorySlot slot;
            std::uint8_t usage = 0;
            std::array results{Number(slot.SlotId), Number(slot.StockId), Number(slot.ItemId), Number(usage), Number(slot.Amount),
                Number(slot.MaxAmount), Number(slot.CashPrice), Number(slot.PurchasedAtUnixSeconds)};
            while (query.Fetch(results))
            {
                if (inventory.Slots.size() == Protocol::CashInventoryUpdateNotification::MaxSlots)
                { throw std::runtime_error("Stored cash inventory is oversized."); }
                slot.Usage = static_cast<InventoryUsage>(usage);
                inventory.Slots.push_back(slot);
            }
            (void)inventory.CreateNotification(accountId).Encode();
            return inventory;
        }

        Features::Characters::CharacterRecord ReadCurrentCharacter(MYSQL* connection,
            const Features::Characters::CharacterRecord& character)
        {
            auto current = character;
            Statement query(connection, "SELECT CharacterType, ColorIndex, Level, Experience, Lucci, Cash, NameUtf16 FROM Characters WHERE CharacterId = ?");
            std::array parameters{Number(character.CharacterId)}; query.Execute(parameters);
            std::array<std::uint8_t, 32> name{};
            unsigned long nameLength = 0;
            std::array results{Number(current.CharacterType), Number(current.ColorIndex), Number(current.Statistics.Level),
                Number(current.Statistics.Experience), Number(current.Statistics.Lucci), Number(current.Statistics.Cash),
                Bytes(name.data(), static_cast<unsigned long>(name.size()))};
            results[6].length = &nameLength;
            if (!query.Fetch(results) || nameLength == 0 || nameLength > name.size() || nameLength % 2 != 0 ||
                current.CharacterType != character.CharacterType || current.ColorIndex != character.ColorIndex)
            { throw std::runtime_error("Inventory transaction character changed."); }
            current.CharacterName.clear();
            for (std::size_t i = 0; i != nameLength; i += 2)
            { current.CharacterName.push_back(static_cast<char16_t>(name[i] | (static_cast<std::uint16_t>(name[i + 1]) << 8))); }
            if (current.CharacterName != character.CharacterName)
            { throw std::runtime_error("Inventory transaction character name changed; reconnect required."); }
            return current;
        }

    }

    MySqlInventoryStore::MySqlInventoryStore(const std::filesystem::path& configuration) : connection_(configuration) {}

    ItemUseResult MySqlInventoryStore::UseItem(const Features::Characters::CharacterRecord& character,
        const Protocol::ItemUseRequest& request, std::uint64_t observedAtUnixSeconds)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, character.AccountId, character.CharacterId))
        { throw std::runtime_error("Item use owner is unavailable."); }
        const auto current = ReadCurrentCharacter(connection, character);
        const auto before = LoadSlots(connection, character.AccountId, character.CharacterId);
        auto result = ItemUse::PrepareRename(current, before, request, observedAtUnixSeconds);
        if (result.Response.Result != Protocol::ItemUseResponse::RenameSuccess) { return result; }
        Protocol::PacketWriter name;
        for (const auto unit : result.Character.CharacterName) { name.WriteUInt16(static_cast<std::uint16_t>(unit)); }
        const auto bytes = name.TakeBytes();
        try
        {
            Statement rename(connection, "UPDATE Characters SET NameUtf16 = ? WHERE CharacterId = ? AND AccountId = ?");
            std::array parameters{Bytes(bytes.data(), static_cast<unsigned long>(bytes.size())),
                Number(character.CharacterId), Number(character.AccountId)};
            rename.Execute(parameters);
            if (rename.AffectedRows() != 1) { throw std::runtime_error("Character rename did not update one row."); }
        }
        catch (const MySqlError& error)
        {
            // The unique UTF-16 byte index arbitrates concurrent different
            // owners without a racy pre-check. Transaction destruction rolls
            // back before the caller can publish the refusal.
            if (error.Code() == 1062)
            { return ItemUse::PrepareRename(current, before, request, observedAtUnixSeconds, true); }
            throw;
        }
        auto& slot = *std::find_if(result.Inventory.Slots.begin(), result.Inventory.Slots.end(),
            [&](const auto& value) { return value.ItemId == request.ItemId; });
        // Loaded slots always have a nonzero revision. The complete response
        // batch was encoded before either write; a later failure rolls back both.
        PersistGrant(connection, current, slot);
        transaction.Commit();
        return result;
    }

    std::optional<CharacterInventory> MySqlInventoryStore::Load(std::uint64_t authenticatedAccountId,
        std::uint64_t characterId, std::uint64_t observedAtUnixSeconds)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, authenticatedAccountId, characterId)) { return {}; }
        auto inventory = LoadSlots(connection, authenticatedAccountId, characterId);
        (void)inventory.ToRecords(authenticatedAccountId, observedAtUnixSeconds);
        transaction.Commit();
        return inventory;
    }

    EquipmentChangeResult MySqlInventoryStore::ChangeEquipment(const Features::Characters::CharacterRecord& character,
        std::uint64_t itemIdentifier, bool equip, std::uint64_t observedAtUnixSeconds)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, character.AccountId, character.CharacterId))
        { throw std::runtime_error("Equipment owner is unavailable."); }
        {
            // The session profile may predate another trusted writer. Do not
            // authorize equipment against stale model, color or level values.
            Statement query(connection, "SELECT CharacterType, ColorIndex, Level FROM Characters WHERE CharacterId = ?");
            std::array parameters{Number(character.CharacterId)};
            query.Execute(parameters);
            std::uint32_t type = 0, color = 0, level = 0;
            std::array results{Number(type), Number(color), Number(level)};
            if (!query.Fetch(results) || type != character.CharacterType || color != character.ColorIndex || level != character.Statistics.Level)
            { throw std::runtime_error("Equipment character profile changed."); }
        }
        const auto before = LoadSlots(connection, character.AccountId, character.CharacterId);
        auto prepared = EquipmentChange::Prepare(character, before, itemIdentifier, equip, observedAtUnixSeconds);
        auto presets = LoadPresets(connection, before);
        presets.CaptureActive(prepared.Inventory);
        prepared.Packets.push_back(presets.CreateNotification(character, prepared.Inventory, observedAtUnixSeconds).Encode());
        PersistEquipment(connection, before, prepared.Inventory);
        PersistPresets(connection, character.CharacterId, presets);
        transaction.Commit();
        return prepared;
    }

    WeaponPresetChangeResult MySqlInventoryStore::ChangeWeaponPreset(const Features::Characters::CharacterRecord& character,
        const Protocol::WeaponPresetChangeRequest& request, std::uint64_t observedAtUnixSeconds)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, character.AccountId, character.CharacterId))
        { throw std::runtime_error("Weapon preset owner is unavailable."); }
        const auto current = ReadCurrentCharacter(connection, character);
        const auto before = LoadSlots(connection, character.AccountId, character.CharacterId);
        auto prepared = PrepareWeaponPresetChange(current, before, LoadPresets(connection, before), request, observedAtUnixSeconds);
        // The complete batch is already encoded. A failure in either write
        // rolls back both the equipment revisions and all preset state.
        PersistEquipment(connection, before, prepared.Inventory);
        PersistPresets(connection, character.CharacterId, prepared.Presets);
        transaction.Commit();
        return prepared;
    }

    Features::Shop::ShopPurchaseResult MySqlInventoryStore::Purchase(const Features::Characters::CharacterRecord& character,
        const Protocol::ShopPurchaseRequest& request, std::uint64_t observedAtUnixSeconds)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, character.AccountId, character.CharacterId))
        { throw std::runtime_error("Purchase owner is unavailable."); }
        const auto current = ReadCurrentCharacter(connection, character);
        const auto inventory = LoadSlots(connection, character.AccountId, character.CharacterId);
        const auto* stock = Features::Shop::ShopStockCatalog::Find(request.StockId);
        const auto* offer = stock ? Features::Shop::ShopCatalog::FindItem(stock->ItemId) : nullptr;
        if (offer && (offer->Currency == Features::Shop::ShopCurrency::Cash || offer->Currency == Features::Shop::ShopCurrency::CashCn))
        {
            auto pending = LoadCashSlots(connection, character.AccountId, character.CharacterId);
            const auto previousPendingCount = pending.Slots.size();
            std::uint32_t assignedId = 0;
            {
                Statement allocation(connection, "SELECT NextSlotId FROM CashInventorySequence WHERE SequenceId = 1 FOR UPDATE");
                std::array<MYSQL_BIND, 0> parameters{}; allocation.Execute(parameters);
                std::array results{Number(assignedId)};
                if (!allocation.Fetch(results)) { throw std::runtime_error("Missing cash inventory sequence."); }
            }
            auto result = Features::Shop::CashShop::Purchase(request, current, inventory, std::move(pending), assignedId, observedAtUnixSeconds);
            if (result.Purchase.Response.Result != Protocol::ShopPurchaseResponse::Success) { return result.Purchase; }
            if (result.CashCost != 0)
            {
                Statement debit(connection, "UPDATE Characters SET Cash = ? WHERE CharacterId = ? AND AccountId = ? AND Cash = ?");
                std::array parameters{Number(result.Purchase.Character.Statistics.Cash), Number(character.CharacterId),
                    Number(character.AccountId), Number(current.Statistics.Cash)}; debit.Execute(parameters);
                if (debit.AffectedRows() != 1) { throw std::runtime_error("Cash purchase balance changed."); }
            }
            for (auto index = previousPendingCount; index < result.Pending.Slots.size(); ++index)
            {
                const auto& slot = result.Pending.Slots[index];
                const auto usage = static_cast<std::uint8_t>(slot.Usage);
                Statement insert(connection, "INSERT INTO CashInventorySlots (SlotId, CharacterId, StockId, ItemId, UsageKind, Amount, "
                    "MaxAmount, CashPrice, PurchasedAtUnixSeconds) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
                std::array parameters{Number(slot.SlotId), Number(character.CharacterId), Number(slot.StockId), Number(slot.ItemId), Number(usage),
                    Number(slot.Amount), Number(slot.MaxAmount), Number(slot.CashPrice), Number(slot.PurchasedAtUnixSeconds)};
                insert.Execute(parameters);
                if (insert.AffectedRows() != 1) { throw std::runtime_error("Cash slot insert failed."); }
            }
            const auto allocatedCount = static_cast<std::uint32_t>(result.Pending.Slots.size() - previousPendingCount);
            Statement advance(connection, "UPDATE CashInventorySequence SET NextSlotId = NextSlotId + ? WHERE SequenceId = 1 AND NextSlotId = ?");
            std::array advanceParameters{Number(allocatedCount), Number(assignedId)}; advance.Execute(advanceParameters);
            if (advance.AffectedRows() != 1) { throw std::runtime_error("Cash slot allocation changed."); }
            transaction.Commit();
            return result.Purchase;
        }
        auto result = Features::Shop::ShopPurchase::Prepare(request, current, inventory, observedAtUnixSeconds);
        if (result.Response.Result != Protocol::ShopPurchaseResponse::Success) { return result; }
        auto& slot = *std::find_if(result.Inventory.Slots.begin(), result.Inventory.Slots.end(),
            [&](const auto& item) { return item.ItemId == result.ChangedItemId; });
        if (result.LucciCost != 0)
        {
            Statement debit(connection, "UPDATE Characters SET Lucci = ? WHERE CharacterId = ? AND AccountId = ? AND Lucci = ?");
            std::array parameters{Number(result.Character.Statistics.Lucci), Number(character.CharacterId),
                Number(character.AccountId), Number(current.Statistics.Lucci)};
            debit.Execute(parameters);
            if (debit.AffectedRows() != 1) { throw std::runtime_error("Purchase balance changed during transaction."); }
        }
        PersistGrant(connection, character, slot);
        transaction.Commit();
        return result;
    }

    std::optional<InventorySnapshot> MySqlInventoryStore::LoadSnapshot(std::uint64_t authenticatedAccountId,
        std::uint64_t characterId, std::uint64_t observedAtUnixSeconds)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, authenticatedAccountId, characterId)) { return {}; }
        InventorySnapshot snapshot{LoadSlots(connection, authenticatedAccountId, characterId),
            LoadCashSlots(connection, authenticatedAccountId, characterId)};
        (void)snapshot.Inventory.ToRecords(authenticatedAccountId, observedAtUnixSeconds);
        snapshot.Presets = LoadPresets(connection, snapshot.Inventory);
        transaction.Commit();
        return snapshot;
    }

    std::optional<InventorySnapshot> MySqlInventoryStore::LoadMenuSnapshot(
        const Features::Characters::CharacterRecord& character, std::uint64_t observedAtUnixSeconds)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, character.AccountId, character.CharacterId)) { return {}; }
        const auto current = ReadCurrentCharacter(connection, character);
        InventorySnapshot snapshot{DefaultInventory::Prepare(current,
            LoadSlots(connection, current.AccountId, current.CharacterId), observedAtUnixSeconds),
            LoadCashSlots(connection, current.AccountId, current.CharacterId)};
        snapshot.Presets = LoadPresets(connection, snapshot.Inventory);
        (void)snapshot.Presets.CreateNotification(current, snapshot.Inventory, observedAtUnixSeconds).Encode();
        bool inserted = false;
        for (auto& slot : snapshot.Inventory.Slots)
        {
            if (slot.Revision != 0) { continue; }
            PersistGrant(connection, current, slot);
            inserted = true;
        }
        if (inserted) { PersistPresets(connection, current.CharacterId, snapshot.Presets); }
        transaction.Commit();
        return snapshot;
    }

    Features::Shop::CashPickupResult MySqlInventoryStore::Pickup(const Features::Characters::CharacterRecord& character,
        const Protocol::CashInventoryPickupRequest& request, std::uint64_t observedAtUnixSeconds)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, character.AccountId, character.CharacterId))
        { throw std::runtime_error("Cash pickup owner is unavailable."); }
        const auto current = ReadCurrentCharacter(connection, character);
        auto result = Features::Shop::CashShop::Pickup(request, current,
            LoadSlots(connection, character.AccountId, character.CharacterId),
            LoadCashSlots(connection, character.AccountId, character.CharacterId), observedAtUnixSeconds);
        if (result.Response.Result != Protocol::CashInventoryPickupResponse::Success) { return result; }
        auto& slot = *std::find_if(result.Inventory.Slots.begin(), result.Inventory.Slots.end(),
            [&](const auto& value) { return value.ItemId == result.ChangedItemId; });
        PersistGrant(connection, character, slot);
        Statement remove(connection, "DELETE FROM CashInventorySlots WHERE SlotId = ? AND CharacterId = ?");
        std::array parameters{Number(request.SlotId), Number(character.CharacterId)}; remove.Execute(parameters);
        if (remove.AffectedRows() != 1) { throw std::runtime_error("Cash pickup slot changed."); }
        transaction.Commit();
        return result;
    }

    bool MySqlInventoryStore::Save(std::uint64_t authenticatedAccountId, std::uint64_t characterId,
        InventorySlot& slot, std::uint64_t observedAtUnixSeconds)
    {
        (void)slot.ToProtocol(observedAtUnixSeconds);
        if (slot.Revision == (std::numeric_limits<std::uint64_t>::max)())
        { throw std::invalid_argument("Inventory revision exhausted."); }
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        if (!LockOwner(connection, authenticatedAccountId, characterId)) { return false; }
        auto inventory = LoadSlots(connection, authenticatedAccountId, characterId);
        const auto found = std::find_if(inventory.Slots.begin(), inventory.Slots.end(),
            [&](const auto& current) { return current.ItemId == slot.ItemId; });
        if (found == inventory.Slots.end())
        {
            if (slot.Revision != 0) { return false; }
            inventory.Slots.push_back(slot);
        }
        else
        {
            if (slot.Revision == 0 || found->Revision != slot.Revision) { return false; }
            if (found->Usage != slot.Usage)
            { throw std::invalid_argument("Inventory usage is a catalog property, not a mutable slot value."); }
            *found = slot;
        }
        (void)inventory.ToRecords(authenticatedAccountId, observedAtUnixSeconds);

        Protocol::InventoryItemRecord detail;
        detail.TextAt36 = slot.TextAt36;
        detail.AttributesAt24 = slot.AttributesAt24;
        const auto bytes = Protocol::InventoryItemRecord::EncodeList(std::span(&detail, 1));
        const auto usage = static_cast<std::uint8_t>(slot.Usage);
        const std::uint8_t equipped = slot.Equipped ? 1 : 0;
        const auto expiry = slot.ExpiresAtUnixSeconds.value_or(0);
        bool expiryNull = !slot.ExpiresAtUnixSeconds;
        const auto revision = slot.Revision + 1;
        if (slot.Revision == 0)
        {
            Statement insert(connection, "INSERT INTO InventorySlots (UsageKind, Quantity, ExpiresAtUnixSeconds, IsEquipped, "
                "DetailData, Revision, CharacterId, ItemId, DetailVersion) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1)");
            std::array parameters{Number(usage), Number(slot.Quantity), Number(expiry), Number(equipped),
                Bytes(bytes.data(), static_cast<unsigned long>(bytes.size())), Number(revision), Number(characterId), Number(slot.ItemId)};
            parameters[2].is_null = &expiryNull;
            insert.Execute(parameters);
            if (insert.AffectedRows() != 1) { throw std::runtime_error("Inventory insert did not affect one row."); }
        }
        else
        {
            Statement update(connection, "UPDATE InventorySlots SET UsageKind = ?, Quantity = ?, ExpiresAtUnixSeconds = ?, "
                "IsEquipped = ?, DetailData = ?, Revision = ?, DetailVersion = 1 WHERE CharacterId = ? AND ItemId = ? AND Revision = ?");
            std::array parameters{Number(usage), Number(slot.Quantity), Number(expiry), Number(equipped),
                Bytes(bytes.data(), static_cast<unsigned long>(bytes.size())), Number(revision), Number(characterId), Number(slot.ItemId), Number(slot.Revision)};
            parameters[2].is_null = &expiryNull;
            update.Execute(parameters);
            if (update.AffectedRows() != 1) { return false; }
        }
        transaction.Commit();
        slot.Revision = revision;
        return true;
    }
}
