#include "TcpServer.h"

#include "Protocol/AdditiveStreamCipher.h"
#include "Protocol/ConnectionGreeting.h"
#include "Protocol/PacketStreamDecoder.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>

#include <array>
#include <iostream>
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

        void ReceiveConnection(SOCKET socket, PacketHandler handlePacket, std::uint32_t idleTimeoutSeconds)
        {
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
            for (;;)
            {
                const int received = recv(socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
                if (received == SOCKET_ERROR)
                {
                    ThrowSocketError("recv");
                }
                if (received == 0)
                {
                    decoder.Finish();
                    std::cout << "Connection closed. Packets=" << packetCount << std::endl;
                    return;
                }
                auto bytes = std::span(buffer).first(static_cast<std::size_t>(received));
                cipher.Decode(bytes);
                for (const auto& packet : decoder.Feed(bytes))
                {
                    ++packetCount;
                    // Payloads can contain credentials. Log metadata only.
                    std::cout << "ClientToServer MessageId=" << packet.MessageId
                              << " PayloadBytes=" << packet.Payload.size() << std::endl;
                    const auto outcome = handlePacket(packet);
                    for (const auto& response : outcome.Responses)
                    {
                        auto outgoing = Protocol::TcpPacketCodec::Encode(response);
                        outgoingCipher.Encode(outgoing);
                        SendAll(socket, outgoing);
                        std::cout << "ServerToClient MessageId=" << response.MessageId
                                  << " PayloadBytes=" << response.Payload.size() << std::endl;
                    }
                    if (outcome.CloseConnection)
                    {
                        shutdown(socket, SD_SEND);
                        std::cout << "Connection closed by message handler. Packets=" << packetCount << std::endl;
                        return;
                    }
                }
            }
        }
    }

    int TcpServer::Run(std::uint16_t port, bool once, const PacketHandlerFactory& createHandler, std::uint32_t idleTimeoutSeconds) const
    {
        if (idleTimeoutSeconds == 0 || idleTimeoutSeconds > 600) { throw std::invalid_argument("Invalid idle timeout."); }
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
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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
        std::cout << "Listening on 127.0.0.1:" << ntohs(address.sin_port) << std::endl;
        do
        {
            const SocketHandle connection(accept(listener.Get(), nullptr, nullptr));
            try
            {
                ReceiveConnection(connection.Get(), createHandler(), idleTimeoutSeconds);
            }
            catch (const std::exception& error)
            {
                std::cerr << "Connection rejected: " << error.what() << std::endl;
                if (once)
                {
                    return 1;
                }
            }
        } while (!once);
        return 0;
    }
}
