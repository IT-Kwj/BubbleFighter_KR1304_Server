#include "LoginProfile.h"
#include "Features/Characters/CharacterCreation.h"
#include <bit>
#include <limits>
#include <stdexcept>
#include <string>

namespace BubbleFighter::Server::Features::Authentication
{
    Protocol::LoginResponse LoginProfile::Create(std::uint64_t accountId,
        const Characters::CharacterRecord& character, std::chrono::system_clock::time_point now)
    {
        if (accountId == 0 || character.AccountId != accountId || character.CharacterId == 0 ||
            character.CharacterId > std::numeric_limits<std::uint32_t>::max() ||
            Characters::CharacterCreation::ValidateName(character.CharacterName) != 0 ||
            !Characters::CharacterCreation::SupportsSelection(character.CharacterType, character.ColorIndex))
        { throw std::runtime_error("Persisted character cannot produce a login profile."); }

        // E019: OLE DATE representation. UTC is this server's explicit clock
        // policy; the original production timezone has not been established.
        const auto epoch = std::chrono::sys_days{std::chrono::year{1899}/12/30};
        const double date = std::chrono::duration<double, std::ratio<86400>>(now - epoch).count();
        if (date < 25569.0 || date >= 2958466.0) { throw std::runtime_error("Server clock outside supported profile range."); }

        Protocol::LoginResponse response;
        response.Result = 0;
        // Local mapping: persistent character id -> 32-bit wire identity.
        // It is checked above, never a truncation of the account primary key.
        response.IdentityValue = static_cast<std::uint32_t>(character.CharacterId);
        auto& success = response.Success.emplace();
        const auto identifier = std::string("LocalAccount.") + std::to_string(accountId);
        success.AccountIdentifier.assign(identifier.begin(), identifier.end());
        success.CharacterName = character.CharacterName;
        success.CharacterType = character.CharacterType;
        success.ServerTimeBits = std::bit_cast<std::uint64_t>(date);
        success.GameMasterValue = 0;

        // E039: preserve the original aggregate constructor's initial values
        // at +64/+68/+72. No direct +64 stack-slot use was found in 9A2770;
        // +68/+72 are copied to connection +2508/+2512. Full meanings unknown.
        success.UnknownValue = 0xFFFFFFFFU;
        success.ChannelCategory = 0;
        success.UnknownConnectionValue = 0;
        // E039: original LoginNational constructor is zero/empty/false. Local
        // accounts supply no external provider metadata or analytics extension.
        response.National.Result = 0;
        response.National.Details.emplace();
        response.TailMarker = 0;
        return response;
    }
}
