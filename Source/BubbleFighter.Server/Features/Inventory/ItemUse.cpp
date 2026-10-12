#include "ItemUse.h"
#include "Features/Characters/CharacterCreation.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Inventory
{
    bool ItemUse::SupportsRename(const ItemDefinition& item) noexcept
    {
        // Current resource has two ordinary Count cards. The Duration card
        // 190488 has a different naming UI and is deliberately not enabled.
        return (item.ItemId == 100104 || item.ItemId == 103368) && item.Enabled &&
            item.Function == 3 && item.Parameter == 0 && item.Usage == InventoryUsage::Count;
    }

    ItemUseResult ItemUse::PrepareRename(const Characters::CharacterRecord& character,
        CharacterInventory inventory, const Protocol::ItemUseRequest& request,
        std::uint64_t observedAtUnixSeconds, bool nameTaken)
    {
        using Response = Protocol::ItemUseResponse;
        (void)request.Encode();
        if (inventory.AccountId != character.AccountId || inventory.CharacterId != character.CharacterId ||
            !character.Statistics.IsValid() || Characters::CharacterCreation::ValidateName(character.CharacterName) != 0 ||
            !Characters::CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex))
        { throw std::invalid_argument("Item use owner/profile mismatch."); }
        (void)inventory.ToRecords(character.AccountId, observedAtUnixSeconds);
        const auto* item = ItemCatalog::Find(request.ItemId);
        if (!item || !SupportsRename(*item) || request.ValueAt4 != 1 || request.Arguments.size() != 1 ||
            character.Statistics.Level < item->RequiredLevel)
        { throw std::invalid_argument("Unsupported item use operation."); }
        ItemUseResult result{character, std::move(inventory), {0, request.ItemId, {}}, {}};
        auto slot = std::find_if(result.Inventory.Slots.begin(), result.Inventory.Slots.end(),
            [&](const auto& row) { return row.ItemId == request.ItemId; });
        if (slot == result.Inventory.Slots.end() || slot->Usage != InventoryUsage::Count || slot->Quantity == 0 ||
            slot->Equipped || slot->Revision == (std::numeric_limits<std::uint64_t>::max)())
        { throw std::invalid_argument("Rename card is unavailable."); }
        const auto& name = request.Arguments.front();
        const auto valid = Characters::CharacterCreation::ValidateName(name);
        if (valid != 0) { result.Response.Result = valid == 6 ? Response::RenameInvalidLength : Response::RenameInvalidCharacters; }
        else if (nameTaken || name == character.CharacterName) { result.Response.Result = Response::RenameNameTaken; }
        if (result.Response.Result != Response::RenameSuccess)
        {
            result.Packets = {result.Response.Encode()};
            return result;
        }
        result.Character.CharacterName = name;
        --slot->Quantity;
        result.Response.Arguments = {name};
        // S813 confirms the name; S802 refreshes the consumed card. The client
        // dialog asks for a relogin to apply the name. S105 is statistics and
        // is NOT assumed to rename the local user. No forced logout is inferred.
        result.Packets = {result.Response.Encode(),
            result.Inventory.CreateNotification(character.AccountId, observedAtUnixSeconds).Encode()};
        return result;
    }
}
