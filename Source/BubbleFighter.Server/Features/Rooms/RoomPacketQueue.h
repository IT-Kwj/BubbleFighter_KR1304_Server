#pragma once
#include "RoomRegistry.h"
#include <memory>
#include <mutex>

namespace BubbleFighter::Server::Features::Rooms
{
    // Registry callbacks only enqueue. The connection thread owns encoding,
    // encryption and socket writes. Overflow fails the connection, not just a packet.
    class RoomPacketQueue : public std::enable_shared_from_this<RoomPacketQueue>
    {
    public:
        RoomPacketSink Sink();
        std::vector<Protocol::TcpPacket> Drain();
        void Clear();
    private:
        void Push(std::vector<Protocol::TcpPacket> packets) noexcept;
        std::mutex mutex_;
        std::vector<Protocol::TcpPacket> packets_;
        std::size_t bytes_ = 0;
        bool failed_ = false;
    };
}
