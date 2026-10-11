#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace BubbleFighter::Server::Features::Authentication
{
    using CredentialDigest = std::array<std::uint8_t, 32>;

    struct PasswordVerifier
    {
        std::array<std::uint8_t, 16> Salt{};
        CredentialDigest Hash{};
        std::uint32_t Iterations = 600000;
    };

    class CredentialCrypto
    {
    public:
        static PasswordVerifier HashPassword(std::string_view password);
        static bool VerifyPassword(std::string_view password, const PasswordVerifier& verifier);
        static CredentialDigest Sha256(std::span<const std::uint8_t> bytes);
        static std::string GenerateTicket();
        static bool IsTicketFormatValid(std::string_view ticket);
        static CredentialDigest HashTicket(std::string_view ticket);
    };
}
