#include "MySqlInventoryStore.h"
#include "MySqlCommands.h"

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
    }

    MySqlInventoryStore::MySqlInventoryStore(const std::filesystem::path& configuration) : connection_(configuration) {}

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
        for (std::size_t index = 0; index != before.Slots.size(); ++index)
        {
            const auto& old = before.Slots[index];
            auto& changed = prepared.Inventory.Slots[index];
            if (old.Equipped == changed.Equipped) { continue; }
            if (old.Revision == (std::numeric_limits<std::uint64_t>::max)())
            { throw std::runtime_error("Equipment revision exhausted."); }
            const std::uint8_t equipped = changed.Equipped ? 1 : 0;
            const auto revision = old.Revision + 1;
            Statement update(connection, "UPDATE InventorySlots SET IsEquipped = ?, Revision = ? "
                "WHERE CharacterId = ? AND ItemId = ? AND Revision = ?");
            std::array parameters{Number(equipped), Number(revision), Number(character.CharacterId), Number(old.ItemId), Number(old.Revision)};
            update.Execute(parameters);
            if (update.AffectedRows() != 1) { throw std::runtime_error("Equipment revision changed during transaction."); }
            changed.Revision = revision;
        }
        transaction.Commit();
        return prepared;
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
