#pragma once

#include "Protocol/TcpPacketCodec.h"

#include <vector>

namespace BubbleFighter::Server::Features::Authentication
{
    enum class InitialHandshakeState
    {
        AwaitingInitialRecord,
        InitialRecordAccepted,
        Rejected
    };

    class InitialHandshake
    {
    public:
        std::vector<Protocol::TcpPacket> Process(const Protocol::TcpPacket& packet);
        InitialHandshakeState State() const;

    private:
        InitialHandshakeState state_ = InitialHandshakeState::AwaitingInitialRecord;
    };
}
