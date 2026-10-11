#pragma once

#include "CharacterStatistics.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace BubbleFighter::Server::Features::Characters
{
    struct CharacterRecord
    {
        std::uint64_t CharacterId = 0;
        std::uint64_t AccountId = 0;
        std::u16string CharacterName;
        std::uint32_t CharacterType = 0;
        std::uint32_t ColorIndex = 0;
        CharacterStatistics Statistics;
        bool operator==(const CharacterRecord&) const = default;
    };

    enum class CharacterCreateResult { Created, NameTaken, AccountHasCharacter, AccountUnavailable };

    class CharacterStore
    {
    public:
        virtual ~CharacterStore() = default;
        virtual std::optional<CharacterRecord> FindByAccount(std::uint64_t accountId) = 0;
        virtual bool NameExists(std::u16string_view name) = 0;
        virtual CharacterCreateResult Create(CharacterRecord& character) = 0;
    };
}
