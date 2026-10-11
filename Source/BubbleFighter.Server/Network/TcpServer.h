#pragma once

#include "Protocol/TcpPacketCodec.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace BubbleFighter::Server::Network
{
    struct PacketHandlingResult
    {
        std::vector<Protocol::TcpPacket> Responses;
        bool CloseConnection = false;
    };

    using PacketHandler = std::function<PacketHandlingResult(const Protocol::TcpPacket&)>;
    using PacketHandlerFactory = std::function<PacketHandler()>;

    class TcpServer
    {
    public:
        // Development transport: loopback only, one connection at a time.
        // Port zero asks Windows to select an available port.
        int Run(std::uint16_t port, bool once, const PacketHandlerFactory& createHandler, std::uint32_t idleTimeoutSeconds = 30) const;
    };
}
