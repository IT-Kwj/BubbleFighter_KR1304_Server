#pragma once

#include "Features/Characters/CharacterStore.h"
#include "Protocol/GotoRoomNotification.h"
#include "Protocol/InventoryViewUpdateNotification.h"

#include <cstddef>
#include <optional>
#include <span>

namespace BubbleFighter::Server::Features::Scenes
{
    // The caller supplies the remaining scene state and a persisted equipment
    // snapshot. These are required inputs, not constructor-default game data.
    struct SceneSlot
    {
        Protocol::SessionMember State;
        std::optional<Characters::CharacterRecord> Character;
        std::optional<Protocol::PartySupplementaryRecord> Equipment;
    };

    class SceneRoster
    {
    public:
        // E050: menu/plaza models have 36 addressable slots. Room capacity also
        // depends on game mode; room snapshots are deliberately not accepted yet.
        static constexpr std::size_t MaxSlots = 36;

        // Populates an unbound menu/plaza envelope. No input is modified, even
        // on failure. This does not authorize sending or initialize game state.
        static Protocol::GotoRoomNotification Compose(
            Protocol::GotoRoomNotification envelope, std::span<const SceneSlot> slots,
            std::uint32_t localSlot, std::uint64_t authenticatedAccountId);

        // E070: narrowly scoped single-player mode 40. This does not admit
        // ordinary room creation, spectators or arbitrary mode capacities.
        static Protocol::GotoRoomNotification ComposePractice(
            Protocol::GotoRoomNotification envelope, const SceneSlot& player,
            std::uint64_t authenticatedAccountId);

        // Ordinary mode-1 room only; shares authenticated identity binding.
        static Protocol::GotoRoomNotification ComposeRoom(
            Protocol::GotoRoomNotification envelope, std::span<const SceneSlot> slots,
            std::uint32_t localSlot, std::uint64_t authenticatedAccountId);

        // Same authoritative identity/character mapping as both S117 maps.
        static Protocol::InventoryViewUpdateNotification CreateInventoryView(
            std::uint64_t authenticatedAccountId, const Characters::CharacterRecord& character,
            const Protocol::PartySupplementaryRecord& equipment);
    private:
        static Protocol::GotoRoomNotification Bind(
            Protocol::GotoRoomNotification envelope, std::span<const SceneSlot> slots,
            std::uint32_t localSlot, std::uint64_t authenticatedAccountId);
    };
}
