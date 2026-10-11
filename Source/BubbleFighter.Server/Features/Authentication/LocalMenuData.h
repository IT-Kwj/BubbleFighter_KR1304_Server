#pragma once

#include "MenuInitialization.h"

namespace BubbleFighter::Server::Features::Authentication
{
    class LocalMenuData
    {
    public:
        // Local solo-menu policy using BFCLIENT02 native constructor defaults.
        // Inventory must have been loaded successfully for this owned character.
        static MenuInitializationData Create(const Characters::CharacterRecord& character,
            Inventory::CharacterInventory inventory, std::uint64_t observedAtUnixSeconds);
    };
}
