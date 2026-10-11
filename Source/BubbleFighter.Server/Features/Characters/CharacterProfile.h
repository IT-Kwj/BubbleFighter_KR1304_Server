#pragma once

#include "CharacterStore.h"
#include "Protocol/UserStatNotification.h"

namespace BubbleFighter::Server::Features::Characters
{
    class CharacterProfile
    {
    public:
        // Binds only E051-confirmed fields. The caller must supply all remaining
        // fields from evidence-backed state; this does not create/send S105.
        static Protocol::UserStatNotification BindKnownFields(Protocol::UserStatNotification envelope,
            const CharacterRecord& character, std::uint64_t authenticatedAccountId);
    };
}
