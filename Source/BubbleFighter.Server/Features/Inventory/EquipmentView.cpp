#include "EquipmentView.h"
#include "ItemCatalog.h"
#include "Features/Characters/CharacterCreation.h"

#include <algorithm>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Inventory
{
    Protocol::PartySupplementaryRecord EquipmentView::Create(const Characters::CharacterRecord& character,
        const CharacterInventory& inventory, std::uint64_t observedAtUnixSeconds)
    {
        if (inventory.AccountId != character.AccountId || inventory.CharacterId != character.CharacterId ||
            !Characters::CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex) ||
            !character.Statistics.IsValid())
        { throw std::invalid_argument("Equipment inventory does not match the persisted character."); }
        // This validates ownership, IDs, duplicates, expiry representation and
        // the cumulative inventory budget before a view can escape.
        const auto records = inventory.ToRecords(character.AccountId, observedAtUnixSeconds);
        Protocol::PartySupplementaryRecord view;
        view.Key = static_cast<std::uint32_t>(character.CharacterId);
        view.Value = 300000U + character.CharacterType;
        std::uint32_t occupiedSlots = 0;
        std::uint32_t occupiedParts = 0;
        for (std::size_t index = 0; index != inventory.Slots.size(); ++index)
        {
            const auto& slot = inventory.Slots[index];
            const auto* definition = ItemCatalog::Find(slot.ItemId);
            if (!definition || definition->Usage != slot.Usage)
            { throw std::invalid_argument("Inventory item is absent from the current catalog or has a mismatched usage."); }
            if (!slot.Equipped) { continue; }
            // Explicit local policy for the currently supported ordinary path.
            // Special functions, pets, titles and ArticleSlot instances need
            // their own state; never silently render them as ordinary gear.
            if (!definition->Enabled || definition->Function != 0 || slot.ItemId < 100000 || slot.ItemId >= 301000 ||
                (records[index].Flags & 2) != 0 || character.Statistics.Level < definition->RequiredLevel ||
                (definition->AllowedCharacters & (std::uint32_t{1} << character.CharacterType)) == 0)
            { throw std::invalid_argument("Equipped item is unsupported, expired or ineligible for this character."); }
            const bool isCharacter = slot.ItemId >= 300000;
            if (isCharacter && slot.ItemId != view.Value)
            { throw std::invalid_argument("Equipped character differs from the persisted current character."); }
            const auto parts = definition->PartMask | definition->SecondaryPartMask;
            if ((!isCharacter && definition->SlotMask == 0 && parts == 0) ||
                (occupiedSlots & definition->SlotMask) != 0 || (occupiedParts & parts) != 0)
            { throw std::invalid_argument("Equipped items have unknown or conflicting positions."); }
            occupiedSlots |= definition->SlotMask;
            occupiedParts |= parts;
            // 0x00817DF0 consumes the first uint32 vector after the model ID,
            // invoking 0x00817680 for each catalog item. S117 and S806 reuse it.
            view.Groups[0].push_back(slot.ItemId);
        }
        std::sort(view.Groups[0].begin(), view.Groups[0].end());
        return view;
    }
}
