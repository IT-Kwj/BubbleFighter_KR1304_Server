#pragma once

#include "Features/Characters/CharacterStore.h"
#include "Protocol/ShopEntryRequest.h"
#include "Protocol/ShopEntryResponse.h"

namespace BubbleFighter::Server::Features::Shop
{
    class LocalShopCatalog
    {
    public:
        // A full resource-derived snapshot. Session code decides whether the
        // client already owns this revision; no account balance or inventory
        // is changed by browsing. Purchase policy is a separate operation.
        static Protocol::ShopEntryResponse Create(const Protocol::ShopEntryRequest& request,
            const Characters::CharacterRecord& character, std::uint64_t observedAtUnixSeconds);
    };
}
