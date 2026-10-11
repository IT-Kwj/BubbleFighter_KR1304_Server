#pragma once

#include "AccountStore.h"

namespace BubbleFighter::Server::Features::Authentication
{
    class LocalAuthentication
    {
    public:
        static constexpr std::uint32_t TicketLifetimeSeconds = 300;

        explicit LocalAuthentication(AccountStore& store);
        // Local administrative operation. This is not a public registration API.
        bool CreateAccount(std::string_view username, std::string_view password);
        std::optional<std::string> Login(std::string_view username, std::string_view password);
        std::optional<std::uint64_t> ConsumeTicket(std::string_view ticket);
        static std::string NormalizeUsername(std::string_view username);

    private:
        AccountStore& store_;
    };
}
