#include "TcpServer.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>

#include "Protocol/AdditiveStreamCipher.h"
#include "Protocol/ConnectionGreeting.h"
#include "Protocol/PacketStreamDecoder.h"
#include "Protocol/ShopEntryResponse.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>

#include <array>
#include <chrono>
#include <charconv>
#include <iostream>
#include <syncstream>
#include <stdexcept>
#include <string>

namespace BubbleFighter::Server::Network
{
    namespace
    {
        [[noreturn]] void ThrowSocketError(const char* operation)
        {
            throw std::runtime_error(std::string(operation) + " failed: Winsock " + std::to_string(WSAGetLastError()));
        }

        class WinsockSession
        {
        public:
            WinsockSession()
            {
                WSADATA data{};
                const int error = WSAStartup(MAKEWORD(2, 2), &data);
                if (error != 0)
                {
                    throw std::runtime_error("WSAStartup failed: " + std::to_string(error));
                }
            }
            ~WinsockSession() { WSACleanup(); }
            WinsockSession(const WinsockSession&) = delete;
            WinsockSession& operator=(const WinsockSession&) = delete;
        };

        class SocketHandle
        {
        public:
            explicit SocketHandle(SOCKET value) : value_(value)
            {
                if (value == INVALID_SOCKET)
                {
                    ThrowSocketError("socket/accept");
                }
            }
            ~SocketHandle() { closesocket(value_); }
            SocketHandle(const SocketHandle&) = delete;
            SocketHandle& operator=(const SocketHandle&) = delete;
            SOCKET Get() const { return value_; }

        private:
            SOCKET value_;
        };

        void SendAll(SOCKET socket, std::span<const std::uint8_t> bytes)
        {
            std::size_t sent = 0;
            while (sent < bytes.size())
            {
                const auto result = send(socket, reinterpret_cast<const char*>(bytes.data() + sent),
                    static_cast<int>(bytes.size() - sent), 0);
                if (result == SOCKET_ERROR)
                {
                    ThrowSocketError("send");
                }
                if (result == 0)
                {
                    throw std::runtime_error("Connection closed while sending data.");
                }
                sent += static_cast<std::size_t>(result);
            }
        }

        void ReceiveConnection(SOCKET socket, ConnectionHandlers handlers, std::uint32_t idleTimeoutSeconds,
            std::stop_token stop = {})
        {
            if (!handlers.OnPacket) { throw std::invalid_argument("Missing packet handler."); }
            // Local resource policy; original server timeout is not established.
            const DWORD timeoutMilliseconds = idleTimeoutSeconds * 1000;
            for (const auto option : {SO_RCVTIMEO, SO_SNDTIMEO})
            {
                if (setsockopt(socket, SOL_SOCKET, option, reinterpret_cast<const char*>(&timeoutMilliseconds),
                    sizeof(timeoutMilliseconds)) == SOCKET_ERROR)
                {
                    ThrowSocketError("setsockopt timeout");
                }
            }
            SendAll(socket, Protocol::ConnectionGreeting::Create());
            Protocol::AdditiveStreamCipher cipher(Protocol::WireDirection::ClientToServer);
            Protocol::AdditiveStreamCipher outgoingCipher(Protocol::WireDirection::ServerToClient);
            Protocol::PacketStreamDecoder decoder;
            std::array<std::uint8_t, 4096> buffer{};
            std::size_t packetCount = 0;
            const auto deliver = [&](const PacketHandlingResult& outcome)
            {
                for (const auto& response : outcome.Responses)
                {
                    auto outgoing = Protocol::TcpPacketCodec::Encode(response,
                        response.MessageId == Protocol::ShopEntryResponse::MessageId ?
                        static_cast<std::uint32_t>(Protocol::ShopEntryResponse::MaxPayloadBytes) :
                        Protocol::TcpPacketCodec::DefaultMaxPayloadSize);
                    outgoingCipher.Encode(outgoing);
                    SendAll(socket, outgoing);
                    std::osyncstream(std::cout) << "ServerToClient MessageId=" << response.MessageId
                              << " PayloadBytes=" << response.Payload.size() << std::endl;
                }
                if (outcome.CloseConnection)
                {
                    shutdown(socket, SD_SEND);
                    std::osyncstream(std::cout) << "Connection closed by message handler. Packets=" << packetCount << std::endl;
                }
                return outcome.CloseConnection;
            };
            auto lastReceive = std::chrono::steady_clock::now();
            while (!stop.stop_requested())
            {
                // Serialized tick and packet callbacks share this connection's
                // cipher stream. No sleeps in business handlers or timer thread.
                if (handlers.OnTick && deliver(handlers.OnTick())) { return; }
                fd_set readable;
                FD_ZERO(&readable);
                FD_SET(socket, &readable);
                timeval interval{0, 100000};
                const int available = select(0, &readable, nullptr, nullptr, &interval);
                if (available == SOCKET_ERROR) { ThrowSocketError("select"); }
                if (available == 0)
                {
                    if (std::chrono::steady_clock::now() - lastReceive >= std::chrono::seconds{idleTimeoutSeconds})
                    { throw std::runtime_error("Connection receive idle timeout."); }
                    continue;
                }
                const int received = recv(socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
                if (received == SOCKET_ERROR)
                {
                    ThrowSocketError("recv");
                }
                if (received == 0)
                {
                    decoder.Finish();
                    std::osyncstream(std::cout) << "Connection closed. Packets=" << packetCount << std::endl;
                    return;
                }
                lastReceive = std::chrono::steady_clock::now();
                auto bytes = std::span(buffer).first(static_cast<std::size_t>(received));
                cipher.Decode(bytes);
                for (const auto& packet : decoder.Feed(bytes))
                {
                    ++packetCount;
                    // Payloads can contain credentials. Log metadata only.
                    std::osyncstream(std::cout) << "ClientToServer MessageId=" << packet.MessageId
                              << " PayloadBytes=" << packet.Payload.size() << std::endl;
                    if (deliver(handlers.OnPacket(packet))) { return; }
                }
            }
        }
    }

    std::uint32_t TcpServer::ParseListenAddress(std::string_view address)
    {
        std::uint32_t result = 0;
        for (int part = 0; part < 4; ++part)
        {
            const auto dot = address.find('.');
            const auto digits = address.substr(0, dot);
            if (digits.empty() || digits.size() > 3 || (digits.size() > 1 && digits.front() == '0') ||
                (part == 3) != (dot == std::string_view::npos))
            { throw std::invalid_argument("Listen address must be dotted-decimal IPv4."); }
            unsigned int value = 0;
            const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || value > 255)
            { throw std::invalid_argument("Listen address must be dotted-decimal IPv4."); }
            result = (result << 8) | value;
            if (part != 3) { address.remove_prefix(dot + 1); }
        }
        return result;
    }

    int TcpServer::Run(std::uint16_t port, bool once, const PacketHandlerFactory& createHandler,
        std::uint32_t idleTimeoutSeconds, const std::function<void(std::uint16_t)>& onListening,
        std::stop_token stop, std::string_view listenAddress) const
    {
        if (idleTimeoutSeconds == 0 || idleTimeoutSeconds > 600) { throw std::invalid_argument("Invalid idle timeout."); }
        const auto ipv4 = ParseListenAddress(listenAddress);
        const WinsockSession winsock;
        const SocketHandle listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        const BOOL exclusiveAddress = TRUE;
        if (setsockopt(listener.Get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<const char*>(&exclusiveAddress), sizeof(exclusiveAddress)) == SOCKET_ERROR)
        {
            ThrowSocketError("setsockopt exclusive address");
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(ipv4);
        address.sin_port = htons(port);
        if (bind(listener.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
        {
            ThrowSocketError("bind");
        }
        if (listen(listener.Get(), SOMAXCONN) == SOCKET_ERROR)
        {
            ThrowSocketError("listen");
        }
        int addressLength = sizeof(address);
        if (getsockname(listener.Get(), reinterpret_cast<sockaddr*>(&address), &addressLength) == SOCKET_ERROR)
        {
            ThrowSocketError("getsockname");
        }
        std::osyncstream(std::cout) << "Listening on " << listenAddress << ':' << ntohs(address.sin_port) << std::endl;
        if (onListening) { onListening(ntohs(address.sin_port)); }
        struct Worker
        {
            std::shared_ptr<SocketHandle> Socket;
            std::shared_ptr<std::atomic_bool> Finished;
            std::jthread Thread;
        };
        std::vector<Worker> workers;
        workers.reserve(64);
        // Joins are bounded by shutdown, even if a peer is not reading replies.
        const auto stopWorkers = [&]
        {
            for (auto& worker : workers)
            {
                worker.Thread.request_stop();
                shutdown(worker.Socket->Get(), SD_BOTH);
            }
            workers.clear();
        };
        try
        {
        while (!stop.stop_requested())
        {
            std::erase_if(workers, [](const Worker& worker) { return worker.Finished->load(); });
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(listener.Get(), &readable);
            timeval interval{0, 100000};
            const auto available = select(0, &readable, nullptr, nullptr, &interval);
            if (available == SOCKET_ERROR) { ThrowSocketError("listener select"); }
            if (available == 0) { continue; }
            auto connection = std::make_shared<SocketHandle>(accept(listener.Get(), nullptr, nullptr));
            if (!once && workers.size() >= 64) { continue; }
            try
            {
                auto handlers = createHandler();
                if (once)
                {
                    ReceiveConnection(connection->Get(), std::move(handlers), idleTimeoutSeconds, stop);
                    return 0;
                }
                auto finished = std::make_shared<std::atomic_bool>(false);
                std::jthread thread([connection, finished, handlers = std::move(handlers), idleTimeoutSeconds]
                    (std::stop_token connectionStop) mutable
                {
                    try { ReceiveConnection(connection->Get(), std::move(handlers), idleTimeoutSeconds, connectionStop); }
                    catch (const std::exception& error)
                    {
                        if (!connectionStop.stop_requested())
                        { std::osyncstream(std::cerr) << "Connection rejected: " << error.what() << std::endl; }
                    }
                    // Destroy session/room leases before announcing completion.
                    handlers = {};
                    finished->store(true);
                });
                workers.push_back({std::move(connection), std::move(finished), std::move(thread)});
            }
            catch (const std::exception& error)
            {
                std::osyncstream(std::cerr) << "Connection rejected: " << error.what() << std::endl;
                if (once)
                {
                    return 1;
                }
            }
        }
        }
        catch (...) { stopWorkers(); throw; }
        stopWorkers();
        return 0;
    }
}
