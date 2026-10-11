#pragma once

#include "CharacterStore.h"
#include "Protocol/CreateUserInfoRequest.h"

namespace BubbleFighter::Server::Features::Characters
{
    class CharacterCreation
    {
    public:
        // Explicit local name policy, not reconstructed production rules.
        static constexpr std::size_t MaxNameCodeUnits = 16;
        static std::uint32_t ValidateName(std::u16string_view name);
        static bool SupportsSelection(std::uint32_t characterType, std::uint32_t colorIndex);
        explicit CharacterCreation(CharacterStore& store);
        std::uint32_t CheckName(std::u16string_view name);
        std::uint32_t Create(std::uint64_t accountId, const Protocol::CreateUserInfoRequest& request);

    private:
        CharacterStore& store_;
    };
}
