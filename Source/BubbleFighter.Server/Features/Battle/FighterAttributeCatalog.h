#pragma once
#include <cstdint>
#include <optional>

namespace BubbleFighter::Server::Features::Battle
{
    enum class FighterAttributeVariant { Normal, Season, BubbleCup };
    struct FighterBaseAttributes
    {
        float Hp, BubbleHp;
        bool operator==(const FighterBaseAttributes&) const = default;
    };
    class FighterAttributeCatalog
    {
    public:
        // Caller supplies native TABLE keys, not arbitrary gameplay mode IDs.
        // No fallback for unknown keys; no ownership grant. Result precedes
        // equipment, skill and runtime modifiers and is not final maximum HP.
        static std::optional<FighterBaseAttributes> Resolve(std::uint32_t character,
            std::uint32_t channelTableKey, std::uint32_t modeTableKey, FighterAttributeVariant variant);
    };
}
