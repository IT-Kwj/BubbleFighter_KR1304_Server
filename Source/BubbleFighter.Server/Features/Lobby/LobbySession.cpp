#include "LobbySession.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Lobby
{
    Protocol::GotoLobbyNotification LobbySession::RequestEntry(const Protocol::ClientToServerMessage0x006A&)
    {
        // Local single-party flow: a manual request does not need a party-move
        // notice. This is not proof of original response ordering or plaza data.
        state_ = LobbyEntryState::NotificationSent;
        return {0};
    }

    Protocol::TcpPacket LobbySession::RequestEntry(const Protocol::ClientToServerMessage0x006A&,
        Protocol::GotoRoomNotification scene, Scenes::SceneSlot localPlayer,
        std::uint64_t authenticatedAccountId)
    {
        // BFCLIENT02 0x009AC490 case 3 installs the shared model, copies the
        // scene title and requests LobbyStage. No second stage-only packet.
        if (!localPlayer.Character || !localPlayer.Equipment ||
            localPlayer.Equipment->Key != localPlayer.Character->CharacterId)
        { throw std::invalid_argument("Lobby equipment does not belong to the local character."); }
        scene.Descriptor.SessionType = 3;
        std::array<Scenes::SceneSlot, 1> slots{std::move(localPlayer)};
        auto snapshot = Scenes::SceneRoster::Compose(std::move(scene), slots, 0, authenticatedAccountId);
        auto packet = snapshot.Encode();
        state_ = LobbyEntryState::SnapshotSent;
        return packet;
    }

    LobbyEntryState LobbySession::State() const noexcept { return state_; }
}
