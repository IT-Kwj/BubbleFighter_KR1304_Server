#pragma once
#include "Features/Characters/CharacterStore.h"
#include "Protocol/UpdatePartyNotification.h"

namespace BubbleFighter::Server::Features::Parties
{
    class SoloParty
    {
    public:
        static Protocol::UpdatePartyNotification Create(const Characters::CharacterRecord& character);
    };
}
