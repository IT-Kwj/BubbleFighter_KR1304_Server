#include "SoloParty.h"
#include "Features/Characters/CharacterCreation.h"
#include <limits>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Parties
{
    Protocol::UpdatePartyNotification SoloParty::Create(const Characters::CharacterRecord& character)
    {
        if (character.AccountId == 0 || character.CharacterId == 0 ||
            character.CharacterId > std::numeric_limits<std::uint32_t>::max() ||
            Characters::CharacterCreation::ValidateName(character.CharacterName) != 0)
        { throw std::invalid_argument("Invalid persisted solo party member."); }
        Protocol::UpdatePartyNotification party;
        // E041: state 2 permits the ordinary move path, and member zero is leader.
        // Local policy: a fresh authenticated connection starts as a solo party.
        party.State = 2;
        party.Members.push_back({static_cast<std::uint32_t>(character.CharacterId), character.CharacterName,
            {0xFFFFFFFFU, 0xFFFFFFFFU, 0, 0}});
        // E041: the four opaque fields retain the original PartyMember defaults.
        // No supplementary records are issued until their gameplay meaning is known.
        return party;
    }
}
