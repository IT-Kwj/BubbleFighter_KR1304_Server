#pragma once

#include "TcpServer.h"

#include <chrono>

namespace BubbleFighter::Server::Network
{
    class ConnectionHeartbeat
    {
    public:
        using Clock = std::chrono::steady_clock;
        using Now = std::function<Clock::time_point()>;

        // Serialized by the TCP connection loop. Local interval/response limit:
        // min(10 seconds, receive-idle timeout / 3). No original timing claim.
        static ConnectionHandlers Wrap(ConnectionHandlers handlers, std::function<bool()> authenticated,
            std::uint32_t idleTimeoutSeconds = 30, Now now = Clock::now);
    };
}
