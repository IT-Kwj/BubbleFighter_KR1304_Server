#include "ConnectionHeartbeat.h"
#include "Protocol/ClientPingResponse.h"
#include "Protocol/ServerPingRequest.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Network
{
    namespace
    {
        struct HeartbeatState
        {
            using Clock = ConnectionHeartbeat::Clock;
            ConnectionHandlers Handlers;
            std::function<bool()> Authenticated;
            ConnectionHeartbeat::Now Now;
            std::chrono::milliseconds Interval;
            std::optional<Clock::time_point> LastObserved;
            std::optional<Clock::time_point> LastActivity;
            std::optional<Clock::time_point> PendingSince;
            bool Closed = false;

            Clock::time_point Observe()
            {
                const auto now = Now();
                if (LastObserved && now < *LastObserved) { throw std::runtime_error("Heartbeat clock moved backwards."); }
                LastObserved = now;
                return now;
            }

            bool Expired(Clock::time_point now)
            {
                if ((LastActivity && !Authenticated()) || (PendingSince && now - *PendingSince >= Interval))
                { Closed = true; }
                return Closed;
            }

            void Activate(Clock::time_point now)
            {
                if (!LastActivity && Authenticated()) { LastActivity = now; }
            }

            PacketHandlingResult Process(const Protocol::TcpPacket& packet)
            {
                if (Closed) { return {{}, true}; }
                const auto now = Observe();
                if (Expired(now)) { return {{}, true}; }
                if (packet.MessageId == Protocol::ClientPingResponse::MessageId)
                {
                    Protocol::ClientPingResponse::Decode(packet);
                    if (!LastActivity || !PendingSince)
                    { throw std::invalid_argument("Unsolicited client ping response."); }
                    LastActivity = now;
                    PendingSince.reset();
                    return {};
                }
                auto result = Handlers.OnPacket(packet);
                if (result.CloseConnection) { Closed = true; return result; }
                const auto after = Observe();
                if (Expired(after)) { return {{}, true}; }
                Activate(after);
                return result;
            }

            PacketHandlingResult Poll()
            {
                if (Closed) { return {{}, true}; }
                if (Expired(Observe())) { return {{}, true}; }
                auto result = Handlers.OnTick ? Handlers.OnTick() : PacketHandlingResult{};
                if (result.CloseConnection) { Closed = true; return result; }
                const auto now = Observe();
                if (Expired(now)) { return {{}, true}; }
                Activate(now);
                if (LastActivity && !PendingSince && now - *LastActivity >= Interval)
                {
                    result.Responses.push_back(Protocol::ServerPingRequest{}.Encode());
                    PendingSince = now;
                }
                return result;
            }
        };
    }

    ConnectionHandlers ConnectionHeartbeat::Wrap(ConnectionHandlers handlers, std::function<bool()> authenticated,
        std::uint32_t idleTimeoutSeconds, Now now)
    {
        if (!handlers.OnPacket || !authenticated || !now || idleTimeoutSeconds == 0 || idleTimeoutSeconds > 600)
        { throw std::invalid_argument("Invalid heartbeat configuration."); }
        const auto interval = std::chrono::milliseconds{std::min<std::uint32_t>(10000, idleTimeoutSeconds * 1000 / 3)};
        auto state = std::make_shared<HeartbeatState>();
        state->Handlers = std::move(handlers);
        state->Authenticated = std::move(authenticated);
        state->Now = std::move(now);
        state->Interval = interval;
        ConnectionHandlers result;
        result.OnPacket = [state](const Protocol::TcpPacket& packet)
        {
            try { return state->Process(packet); }
            catch (...) { state->Closed = true; throw; }
        };
        result.OnTick = [state]
        {
            try { return state->Poll(); }
            catch (...) { state->Closed = true; throw; }
        };
        return result;
    }
}
