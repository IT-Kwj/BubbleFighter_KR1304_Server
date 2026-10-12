#pragma once

#include "Features/Authentication/MenuInitialization.h"
#include "Protocol/ClientToServerMessage0x006B.h"

namespace BubbleFighter::Server::Features::Practice
{
    class PracticeEntry
    {
    public:
        // E070 local policy: the current selector-zero map enters native mode
        // 40 (GameTrainingRoomStrategy). Original server selection is unknown.
        // Resource fields are pinned to BFCLIENT02 / version.dat 1304.
        static void Validate(const Protocol::ClientToServerMessage0x006B& request);
        static std::vector<Protocol::TcpPacket> Prepare(
            const Protocol::ClientToServerMessage0x006B& request,
            const Characters::CharacterRecord& character, std::uint64_t authenticatedAccountId,
            Authentication::MenuInitializationData data, std::uint64_t observedAtUnixSeconds);
    };
}
