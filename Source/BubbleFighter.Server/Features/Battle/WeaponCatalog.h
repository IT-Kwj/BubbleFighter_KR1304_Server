#pragma once

#include <cstdint>
#include <span>

namespace BubbleFighter::Server::Features::Battle
{
    // E088: base resource values only. Runtime reinforcement, alternate fire,
    // training-task equipment and inventory ownership are separate concerns.
    struct WeaponDefinition
    {
        std::uint32_t WeaponId;
        std::uint32_t MasterId;
        std::uint32_t WeaponSlotIndex;
        std::uint32_t SpreadCount;
        bool IsSpread;

        bool operator==(const WeaponDefinition&) const = default;
    };

    class WeaponCatalog
    {
    public:
        static std::span<const WeaponDefinition> Entries() noexcept;
        static const WeaponDefinition* Find(std::uint32_t weaponId) noexcept;
        // Native training equipment conversion: item in [201000, 210999]
        // maps to resource item - 200000. A match does not grant the item.
        static const WeaponDefinition* FindEquipmentItem(std::uint32_t itemId) noexcept;
    };
}
