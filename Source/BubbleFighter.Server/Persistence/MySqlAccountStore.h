#pragma once

#include "MySqlConnection.h"
#include "Features/Authentication/AccountStore.h"

namespace BubbleFighter::Server::Persistence
{
    class MySqlAccountStore final : public Features::Authentication::AccountStore
    {
    public:
        explicit MySqlAccountStore(const std::filesystem::path& configurationFile);
        bool CreateAccount(std::string_view normalizedUsername, const Features::Authentication::PasswordVerifier& password) override;
        std::optional<Features::Authentication::StoredAccount> FindAccount(std::string_view normalizedUsername) override;
        bool AddTicket(std::uint64_t accountId, const Features::Authentication::CredentialDigest& digest, std::uint32_t lifetimeSeconds) override;
        std::optional<std::uint64_t> ConsumeTicket(const Features::Authentication::CredentialDigest& digest) override;

    private:
        MySqlConnection connection_;
    };
}
