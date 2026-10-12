#pragma once
#include <cstdint>
#include <span>

namespace BubbleFighter::Server::Features::Battle
{
    struct FighterHealthEquipment
    {
        float Amount = 0;
        float Ratio = 0;
        bool operator==(const FighterHealthEquipment&) const = default;
    };

    class FighterMaximumHealth
    {
    public:
        // 818AA0 applies the character first and each other item once in ID
        // order. Input is the trusted S117 equipment view, never a client hit.
        static FighterHealthEquipment Equipment(std::uint32_t characterType,
            std::span<const std::uint32_t> items, std::uint32_t channel);
        // 480F30, ordinary mode 1: optional runtime scale precedes equipment;
        // Lua scale follows it. Both scales must come from known server state.
        // This does not evaluate other modes or activate skills by itself.
        static float Ordinary(float baseHp, const FighterHealthEquipment& equipment,
            float actorScale, float luaScale);
    };
}
