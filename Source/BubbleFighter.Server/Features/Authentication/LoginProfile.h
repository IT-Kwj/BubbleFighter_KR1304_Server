#pragma once

#include "Features/Characters/CharacterStore.h"
#include "Protocol/LoginResponse.h"
#include <chrono>

namespace BubbleFighter::Server::Features::Authentication
{
    class LoginProfile
    {
    public:
        static Protocol::LoginResponse Create(std::uint64_t trustedAccountId,
            const Characters::CharacterRecord& character, std::chrono::system_clock::time_point now);
    };
}
