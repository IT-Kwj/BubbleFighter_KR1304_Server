#pragma once
#include "Protocol/ClientToServerMessage0x006A.h"
#include "Protocol/GotoLobbyNotification.h"
#include "Features/Scenes/SceneRoster.h"

namespace BubbleFighter::Server::Features::Lobby
{
    enum class LobbyEntryState { AwaitingRequest, NotificationSent, SnapshotSent };

    // Owned only after the authenticated character profile and solo party exist.
    class LobbySession
    {
    public:
        Protocol::GotoLobbyNotification RequestEntry(const Protocol::ClientToServerMessage0x006A&);
        // Full local solo-plaza path: S117 itself selects stage 3. Encoding
        // completes before publishing the new server-side state.
        Protocol::TcpPacket RequestEntry(const Protocol::ClientToServerMessage0x006A&,
            Protocol::GotoRoomNotification scene, Scenes::SceneSlot localPlayer,
            std::uint64_t authenticatedAccountId);
        LobbyEntryState State() const noexcept;
    private:
        LobbyEntryState state_ = LobbyEntryState::AwaitingRequest;
    };
}
