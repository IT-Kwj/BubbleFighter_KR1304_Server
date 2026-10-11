#pragma once

#include <cstdint>

namespace BubbleFighter::Server::Features::Characters
{
    // E051 confirms these field meanings, not the original server's rewards or
    // level curve. New local accounts start at level 1 with no XP or currency.
    struct CharacterStatistics
    {
        static constexpr std::uint32_t MaxValue = 0x7FFFFFFFU;

        std::uint32_t Level = 1;
        std::uint32_t Experience = 0;
        std::uint32_t Lucci = 0;
        std::uint32_t Cash = 0;

        bool IsValid() const;
        bool operator==(const CharacterStatistics&) const = default;
    };
}
