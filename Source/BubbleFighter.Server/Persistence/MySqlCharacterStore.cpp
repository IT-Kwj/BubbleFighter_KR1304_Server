#include "MySqlCharacterStore.h"
#include "MySqlCommands.h"
#include "Features/Characters/CharacterCreation.h"
#include "Protocol/PacketReader.h"
#include "Protocol/PacketWriter.h"

#include <array>

namespace BubbleFighter::Server::Persistence
{
    using namespace Features::Characters;
    using namespace MySqlCommands;

    namespace
    {
        std::vector<std::uint8_t> NameBytes(std::u16string_view name)
        {
            if (CharacterCreation::ValidateName(name) != 0) { throw std::invalid_argument("Invalid character name."); }
            Protocol::PacketWriter writer;
            for (const auto value : name) { writer.WriteUInt16(value); }
            return writer.TakeBytes();
        }
    }

    MySqlCharacterStore::MySqlCharacterStore(const std::filesystem::path& configuration) : connection_(configuration) {}

    std::optional<CharacterRecord> MySqlCharacterStore::FindByAccount(std::uint64_t accountId)
    {
        // A database missing the statistics migration fails here; never silently
        // turn an existing role into a new character or invent missing values.
        Statement query(connection_.Handle(), "SELECT CharacterId, NameUtf16, CharacterType, ColorIndex, Level, Experience, Lucci, Cash FROM Characters WHERE AccountId = ?");
        std::array parameters{Number(accountId)};
        query.Execute(parameters);
        CharacterRecord record;
        record.AccountId = accountId;
        std::array<std::uint8_t, CharacterCreation::MaxNameCodeUnits * 2> name{};
        unsigned long length = 0;
        std::array results{Number(record.CharacterId), Bytes(name.data(), static_cast<unsigned long>(name.size())),
            Number(record.CharacterType), Number(record.ColorIndex), Number(record.Statistics.Level),
            Number(record.Statistics.Experience), Number(record.Statistics.Lucci), Number(record.Statistics.Cash)};
        results[1].length = &length;
        if (!query.Fetch(results)) { return {}; }
        if (length == 0 || length > name.size() || length % 2 != 0 || record.CharacterId == 0) { throw std::runtime_error("Invalid stored character."); }
        Protocol::PacketReader reader(std::span(name).first(length));
        while (reader.RemainingBytes()) { record.CharacterName.push_back(reader.ReadUInt16()); }
        if (CharacterCreation::ValidateName(record.CharacterName) || !CharacterCreation::SupportsSelection(record.CharacterType, record.ColorIndex) ||
            !record.Statistics.IsValid())
        { throw std::runtime_error("Unsupported stored character fields."); }
        return record;
    }

    bool MySqlCharacterStore::NameExists(std::u16string_view name)
    {
        const auto bytes = NameBytes(name);
        Statement query(connection_.Handle(), "SELECT CharacterId FROM Characters WHERE NameUtf16 = ?");
        std::array parameters{Bytes(bytes.data(), static_cast<unsigned long>(bytes.size()))};
        query.Execute(parameters);
        std::uint64_t id = 0;
        std::array results{Number(id)};
        return query.Fetch(results);
    }

    CharacterCreateResult MySqlCharacterStore::Create(CharacterRecord& character)
    {
        const auto name = NameBytes(character.CharacterName);
        if (!CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex)) { throw std::invalid_argument("Unsupported character selection."); }
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        {
            // Serializes creation for the same account; unique name protects
            // cross-account races even when both earlier name checks passed.
            Statement account(connection, "SELECT AccountId FROM Accounts WHERE AccountId = ? AND IsDisabled = 0 FOR UPDATE");
            std::array parameters{Number(character.AccountId)};
            account.Execute(parameters);
            std::uint64_t id = 0;
            std::array results{Number(id)};
            if (!account.Fetch(results)) { return CharacterCreateResult::AccountUnavailable; }
        }
        if (FindByAccount(character.AccountId)) { return CharacterCreateResult::AccountHasCharacter; }
        const CharacterStatistics initialStatistics;
        Statement insert(connection, "INSERT INTO Characters (AccountId, NameUtf16, CharacterType, ColorIndex, Level, Experience, Lucci, Cash) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        std::array parameters{Number(character.AccountId), Bytes(name.data(), static_cast<unsigned long>(name.size())),
            Number(character.CharacterType), Number(character.ColorIndex), Number(initialStatistics.Level),
            Number(initialStatistics.Experience), Number(initialStatistics.Lucci), Number(initialStatistics.Cash)};
        try { insert.Execute(parameters); }
        catch (const MySqlError& error)
        {
            if (error.Code() == 1062) { return CharacterCreateResult::NameTaken; }
            throw;
        }
        if (insert.AffectedRows() != 1) { throw std::runtime_error("Character insert did not affect one row."); }
        const auto characterId = mysql_insert_id(connection);
        transaction.Commit();
        // Statistics use the same local policy as the migration, never values
        // supplied to Create. Publish identity and statistics only on commit.
        character.CharacterId = characterId;
        character.Statistics = initialStatistics;
        return CharacterCreateResult::Created;
    }
}
