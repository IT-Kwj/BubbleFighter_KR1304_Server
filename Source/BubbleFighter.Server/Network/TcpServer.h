#pragma once

#include "Protocol/TcpPacketCodec.h"

#include <cstdint>
#include <functional>
#include <stop_token>
#include <string_view>
#include <vector>

namespace BubbleFighter::Server::Network
{
    struct PacketHandlingResult
    {
        std::vector<Protocol::TcpPacket> Responses;
        bool CloseConnection = false;
    };

    using PacketHandler = std::function<PacketHandlingResult(const Protocol::TcpPacket&)>;
    struct ConnectionHandlers
    {
        PacketHandler OnPacket;
        std::function<PacketHandlingResult()> OnTick;
    };
    using PacketHandlerFactory = std::function<ConnectionHandlers()>;

    class TcpServer
    {
    public:
        // Strict dotted-decimal IPv4, returned in host byte order. This parses
        // an address only; Windows bind checks whether the interface exists.
        static std::uint32_t ParseListenAddress(std::string_view address);
        // At most 64 concurrent clients; loopback is the default interface.
        // Port zero asks Windows to select an available port.
        int Run(std::uint16_t port, bool once, const PacketHandlerFactory& createHandler,
            std::uint32_t idleTimeoutSeconds = 30, const std::function<void(std::uint16_t)>& onListening = {},
            std::stop_token stop = {}, std::string_view listenAddress = "127.0.0.1") const;
    };
}
