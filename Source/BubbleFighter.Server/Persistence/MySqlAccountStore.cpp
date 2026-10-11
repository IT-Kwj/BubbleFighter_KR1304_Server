#include "MySqlAccountStore.h"
#include "MySqlCommands.h"

#include <mysql.h>

#include <array>
#include <span>

namespace BubbleFighter::Server::Persistence
{
    using namespace Features::Authentication;
    using namespace MySqlCommands;

    MySqlAccountStore::MySqlAccountStore(const std::filesystem::path& configurationFile) : connection_(configurationFile) {}

    bool MySqlAccountStore::CreateAccount(std::string_view username, const PasswordVerifier& password)
    {
        if (username.size() > 32) { throw std::invalid_argument("Account username is too long."); }
        Statement statement(connection_.Handle(),
            "INSERT INTO Accounts (Username, PasswordSalt, PasswordHash, PasswordIterations) VALUES (?, ?, ?, ?)");
        std::array parameters{Bytes(username.data(), static_cast<unsigned long>(username.size()), MYSQL_TYPE_STRING),
            Bytes(password.Salt.data(), static_cast<unsigned long>(password.Salt.size())),
            Bytes(password.Hash.data(), static_cast<unsigned long>(password.Hash.size())), Number(password.Iterations)};
        try { statement.Execute(parameters); }
        catch (const MySqlError& error)
        {
            if (error.Code() == 1062) { return false; }
            throw;
        }
        return statement.AffectedRows() == 1;
    }

    std::optional<StoredAccount> MySqlAccountStore::FindAccount(std::string_view username)
    {
        if (username.size() > 32) { return std::nullopt; }
        Statement statement(connection_.Handle(),
            "SELECT AccountId, PasswordSalt, PasswordHash, PasswordIterations, IsDisabled FROM Accounts WHERE Username = ?");
        std::array parameters{Bytes(username.data(), static_cast<unsigned long>(username.size()), MYSQL_TYPE_STRING)};
        statement.Execute(parameters);
        StoredAccount account;
        std::uint8_t disabled = 0;
        unsigned long saltLength = 0;
        unsigned long hashLength = 0;
        std::array results{Number(account.AccountId), Bytes(account.Password.Salt.data(), static_cast<unsigned long>(account.Password.Salt.size())),
            Bytes(account.Password.Hash.data(), static_cast<unsigned long>(account.Password.Hash.size())),
            Number(account.Password.Iterations), Number(disabled)};
        results[1].length = &saltLength;
        results[2].length = &hashLength;
        if (!statement.Fetch(results)) { return std::nullopt; }
        if (saltLength != account.Password.Salt.size() || hashLength != account.Password.Hash.size() || disabled > 1 || account.AccountId == 0)
        {
            throw std::runtime_error("Invalid stored account fields.");
        }
        account.IsDisabled = disabled != 0;
        return account;
    }

    bool MySqlAccountStore::AddTicket(std::uint64_t accountId, const CredentialDigest& digest, std::uint32_t lifetimeSeconds)
    {
        if (lifetimeSeconds == 0 || lifetimeSeconds > 300) { throw std::invalid_argument("Unsupported ticket lifetime."); }
        Statement statement(connection_.Handle(),
            "INSERT INTO LoginTickets (TicketHash, AccountId, ExpiresAt) "
            "SELECT ?, AccountId, TIMESTAMPADD(SECOND, ?, UTC_TIMESTAMP(6)) FROM Accounts WHERE AccountId = ? AND IsDisabled = 0");
        std::array parameters{Bytes(digest.data(), static_cast<unsigned long>(digest.size())), Number(lifetimeSeconds), Number(accountId)};
        statement.Execute(parameters);
        return statement.AffectedRows() == 1;
    }

    std::optional<std::uint64_t> MySqlAccountStore::ConsumeTicket(const CredentialDigest& digest)
    {
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        std::uint64_t accountId = 0;
        std::array parameters{Bytes(digest.data(), static_cast<unsigned long>(digest.size()))};
        {
            Statement query(connection,
                "SELECT t.AccountId FROM LoginTickets t JOIN Accounts a ON a.AccountId = t.AccountId "
                "WHERE t.TicketHash = ? AND t.ConsumedAt IS NULL AND t.ExpiresAt > UTC_TIMESTAMP(6) AND a.IsDisabled = 0 FOR UPDATE");
            query.Execute(parameters);
            std::array results{Number(accountId)};
            if (!query.Fetch(results)) { return std::nullopt; }
        }
        Statement update(connection,
            "UPDATE LoginTickets SET ConsumedAt = UTC_TIMESTAMP(6) "
            "WHERE TicketHash = ? AND ConsumedAt IS NULL AND ExpiresAt > UTC_TIMESTAMP(6)");
        update.Execute(parameters);
        if (update.AffectedRows() != 1) { return std::nullopt; }
        transaction.Commit();
        return accountId;
    }
}
