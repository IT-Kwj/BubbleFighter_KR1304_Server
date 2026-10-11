#pragma once

#include "CredentialCrypto.h"

#include <optional>

namespace BubbleFighter::Server::Features::Authentication
{
    struct StoredAccount
    {
        std::uint64_t AccountId = 0;
        PasswordVerifier Password;
        bool IsDisabled = false;
    };

    class AccountStore
    {
    public:
        virtual ~AccountStore() = default;
        virtual bool CreateAccount(std::string_view normalizedUsername, const PasswordVerifier& password) = 0;
        virtual std::optional<StoredAccount> FindAccount(std::string_view normalizedUsername) = 0;
        virtual bool AddTicket(std::uint64_t accountId, const CredentialDigest& digest, std::uint32_t lifetimeSeconds) = 0;
        // Must atomically reject expired, consumed and disabled-account tickets.
        virtual std::optional<std::uint64_t> ConsumeTicket(const CredentialDigest& digest) = 0;
    };
}
