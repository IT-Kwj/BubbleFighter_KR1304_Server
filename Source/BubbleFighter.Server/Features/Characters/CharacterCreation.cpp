#include "CharacterCreation.h"

#include <array>
#include <utility>

namespace BubbleFighter::Server::Features::Characters
{
    CharacterCreation::CharacterCreation(CharacterStore& store) : store_(store) {}

    std::uint32_t CharacterCreation::ValidateName(std::u16string_view name)
    {
        if (name.empty() || name.size() > MaxNameCodeUnits) { return 6; }
        // Preserve exact code units and case; no silent replacement/normalization.
        // Permit printable Unicode scalars, excluding controls, whitespace,
        // formatting characters and markup metacharacters in this local policy.
        for (std::size_t index = 0; index < name.size(); ++index)
        {
            const auto value = name[index];
            if (value <= 0x20 || (value >= 0x7F && value <= 0xA0) ||
                (value >= 0x2000 && value <= 0x200F) || (value >= 0x2028 && value <= 0x202F) ||
                (value >= 0x2060 && value <= 0x206F) || value == 0x3000 || value == 0xFEFF ||
                value >= 0xFFF0 || value == u'<' || value == u'>' || value == u'&') { return 3; }
            if (value >= 0xD800 && value <= 0xDBFF)
            {
                if (++index == name.size() || name[index] < 0xDC00 || name[index] > 0xDFFF) { return 3; }
            }
            else if (value >= 0xDC00 && value <= 0xDFFF) { return 3; }
        }
        return 0;
    }

    bool CharacterCreation::SupportsSelection(std::uint32_t type, std::uint32_t color)
    {
        // E030: eight actual zoomIn/zoomOut resource pairs. Local supported set;
        // the complete CharacterIndex enum includes non-player types.
        constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 8> supported{{
            {10, 7}, {19, 4}, {25, 6}, {27, 2}, {1, 1}, {2, 6}, {11, 2}, {12, 6}}};
        for (const auto& pair : supported) { if (pair.first == type && pair.second == color) { return true; } }
        return false;
    }

    std::uint32_t CharacterCreation::CheckName(std::u16string_view name)
    {
        if (const auto result = ValidateName(name); result != 0) { return result; }
        return store_.NameExists(name) ? 1u : 0u;
    }

    std::uint32_t CharacterCreation::Create(std::uint64_t accountId, const Protocol::CreateUserInfoRequest& request)
    {
        if (const auto result = ValidateName(request.CharacterName); result != 0) { return result; }
        if (!SupportsSelection(request.CharacterType, request.ColorIndex)) { return 4; }
        // E028 normal create path initializes these to empty/zero. Other variants
        // remain unsupported rather than being silently interpreted.
        if (!request.UnknownString.empty() || request.UnknownValue != 0) { return 2; }
        CharacterRecord record{0, accountId, request.CharacterName, request.CharacterType, request.ColorIndex};
        switch (store_.Create(record))
        {
        case CharacterCreateResult::Created: return 0;
        case CharacterCreateResult::NameTaken: return 1;
        case CharacterCreateResult::AccountHasCharacter: return 2;
        case CharacterCreateResult::AccountUnavailable: return 5;
        }
        return 5;
    }
}
