#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    class PacketStreamDecoder
    {
    public:
        explicit PacketStreamDecoder(std::uint32_t maxPayloadSize = TcpPacketCodec::DefaultMaxPayloadSize);

        // Input is already decrypted. A connection failure poisons this instance.
        std::vector<TcpPacket> Feed(std::span<const std::uint8_t> bytes);
        void Finish();
        bool IsFailed() const;
        bool HasPendingData() const;

    private:
        [[noreturn]] void Fail(const char* message);

        std::uint32_t maxPayloadSize_;
        std::vector<std::uint8_t> buffer_;
        bool failed_ = false;
        bool finished_ = false;
    };
}
