#include "SceneRoster.h"
#include "Features/Characters/CharacterCreation.h"

#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace BubbleFighter::Server::Features::Scenes
{
    namespace
    {
        void ValidateCharacter(const Characters::CharacterRecord& character)
        {
            if (character.AccountId == 0 || character.CharacterId == 0 ||
                character.CharacterId > std::numeric_limits<std::uint32_t>::max() ||
                Characters::CharacterCreation::ValidateName(character.CharacterName) != 0 ||
                !Characters::CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex) ||
                !character.Statistics.IsValid())
            {
                throw std::invalid_argument("Invalid persisted scene character.");
            }
        }

        Protocol::PartySupplementaryRecord BindEquipment(const Characters::CharacterRecord& character,
            const Protocol::PartySupplementaryRecord& equipment, std::uint32_t key)
        {
            // E050: 0x00817500 -> 0x00817DF0 stores Value; 0x00817E50
            // returns that value minus 300000. SupportsSelection bounds the sum.
            // The supplied collections are preserved; no starter items invented.
            auto result = equipment;
            result.Key = key;
            result.Value = 300000U + character.CharacterType;
            return result;
        }
    }

    Protocol::GotoRoomNotification SceneRoster::Compose(Protocol::GotoRoomNotification envelope,
        std::span<const SceneSlot> slots, std::uint32_t localSlot, std::uint64_t authenticatedAccountId)
    {
        if (envelope.Descriptor.SessionType != 1 && envelope.Descriptor.SessionType != 3)
        {
            throw std::invalid_argument("Only menu/plaza scene rosters are supported.");
        }
        if (!envelope.Members.empty() || !envelope.RecordsAt564.empty() || !envelope.RecordsAt588.empty())
        {
            throw std::invalid_argument("Scene envelope already contains a roster.");
        }
        if (slots.empty() || slots.size() > MaxSlots || localSlot >= slots.size() ||
            authenticatedAccountId == 0 || !slots[localSlot].Character ||
            slots[localSlot].Character->AccountId != authenticatedAccountId)
        {
            throw std::invalid_argument("Local scene slot does not belong to the authenticated account.");
        }

        std::unordered_set<std::uint64_t> characterIds;
        std::unordered_set<std::uint64_t> accountIds;
        envelope.Members.reserve(slots.size());
        for (std::size_t index = 0; index < slots.size(); ++index)
        {
            const auto& slot = slots[index];
            auto member = slot.State;
            if (!slot.Character)
            {
                if (slot.Equipment || member.BooleansAt0[0] != 0 || member.ValueAt8 != 0 ||
                    !member.TextAt12.empty() || member.OptionalAt204 ||
                    member.MarkerAt204 == Protocol::GotoRoomNotification::PresentMarker)
                {
                    throw std::invalid_argument("Empty scene slot contains player state.");
                }
            }
            else
            {
                const auto& character = *slot.Character;
                ValidateCharacter(character);
                if (!slot.Equipment || !characterIds.insert(character.CharacterId).second ||
                    !accountIds.insert(character.AccountId).second)
                {
                    throw std::invalid_argument("Duplicate scene player or missing equipment snapshot.");
                }
                // 0x009DA240 excludes members with the optional +204 object.
                // This path handles ordinary persisted players, not that mode.
                if (member.OptionalAt204 || member.MarkerAt204 == Protocol::GotoRoomNotification::PresentMarker)
                {
                    throw std::invalid_argument("Special scene member identity is unsupported.");
                }
                member.BooleansAt0[0] = 1;
                member.ValueAt8 = static_cast<std::uint32_t>(character.CharacterId);
                member.TextAt12 = character.CharacterName;
                // Reader sign-extends this short to member +40. The supported
                // creation choices are positive and fit without truncation.
                member.SignedShortsAt36[1] = static_cast<std::uint16_t>(character.ColorIndex);
                // E051: 0x00912A00 passes member +172 to LevelSymbolIcon.
                member.ValuesAt172[0] = character.Statistics.Level;

                // E050: +564 -> 0x0081D060 uses a slot index, while +588 ->
                // 0x0081CCE0 uses the player identity. Never reuse one key in both.
                envelope.RecordsAt564.push_back(BindEquipment(character, *slot.Equipment,
                    static_cast<std::uint32_t>(index)));
                envelope.RecordsAt588.push_back(BindEquipment(character, *slot.Equipment, member.ValueAt8));
            }
            // Position in the vector is the runtime slot; retain explicit empty
            // entries. Other flags, team, endpoints and opaque fields are inputs.
            envelope.Members.push_back(std::move(member));
        }
        // E050: +728 -> context +580 -> 0x009E4490. +548 is NOT this index.
        envelope.ValueAt728 = localSlot;
        return envelope;
    }

    Protocol::InventoryViewUpdateNotification SceneRoster::CreateInventoryView(
        std::uint64_t authenticatedAccountId, const Characters::CharacterRecord& character,
        const Protocol::PartySupplementaryRecord& equipment)
    {
        ValidateCharacter(character);
        if (authenticatedAccountId == 0 || character.AccountId != authenticatedAccountId)
        {
            throw std::invalid_argument("Inventory view belongs to another account.");
        }
        Protocol::InventoryViewUpdateNotification result;
        result.View = BindEquipment(character, equipment, static_cast<std::uint32_t>(character.CharacterId));
        return result;
    }
}
