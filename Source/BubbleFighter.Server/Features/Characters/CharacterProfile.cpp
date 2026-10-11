#include "CharacterProfile.h"
#include "CharacterCreation.h"

#include <limits>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Characters
{
    Protocol::UserStatNotification CharacterProfile::BindKnownFields(Protocol::UserStatNotification envelope,
        const CharacterRecord& character, std::uint64_t authenticatedAccountId)
    {
        if (authenticatedAccountId == 0 || character.AccountId != authenticatedAccountId ||
            character.CharacterId == 0 || character.CharacterId > std::numeric_limits<std::uint32_t>::max() ||
            CharacterCreation::ValidateName(character.CharacterName) != 0 ||
            !CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex) ||
            !character.Statistics.IsValid())
        {
            throw std::invalid_argument("Invalid persisted character profile or account ownership.");
        }

        // 0x009A5C40 -> local player +56/+68/+76/+80. UI color and named
        // lblLucci/lblCash consumers confirm their meanings (E051).
        envelope.Values[0] = character.ColorIndex;
        envelope.Values[1] = character.Statistics.Experience;
        envelope.Values[4] = character.Statistics.Lucci;
        envelope.Values[5] = character.Statistics.Cash;
        // FighterInfo +36 becomes local player +60; S107 and LevelSymbolIcon
        // independently confirm level. +40 is not a third currency balance.
        envelope.Fighter.ValuesAt32[1] = character.Statistics.Level;
        return envelope;
    }
}
