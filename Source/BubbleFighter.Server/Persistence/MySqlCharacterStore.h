#pragma once

#include "MySqlConnection.h"
#include "Features/Characters/CharacterStore.h"

namespace BubbleFighter::Server::Persistence
{
    class MySqlCharacterStore final : public Features::Characters::CharacterStore
    {
    public:
        explicit MySqlCharacterStore(const std::filesystem::path& configuration);
        std::optional<Features::Characters::CharacterRecord> FindByAccount(std::uint64_t accountId) override;
        bool NameExists(std::u16string_view name) override;
        Features::Characters::CharacterCreateResult Create(Features::Characters::CharacterRecord& character) override;

    private:
        MySqlConnection connection_;
    };
}
