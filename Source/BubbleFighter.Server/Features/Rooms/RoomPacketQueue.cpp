#include "RoomPacketQueue.h"
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Rooms
{
    RoomPacketSink RoomPacketQueue::Sink()
    {
        std::weak_ptr<RoomPacketQueue> self = shared_from_this();
        return [self](std::vector<Protocol::TcpPacket> packets)
        { if (auto queue = self.lock()) { queue->Push(std::move(packets)); } };
    }
    void RoomPacketQueue::Push(std::vector<Protocol::TcpPacket> packets) noexcept
    {
        std::lock_guard lock(mutex_);
        if (failed_) { return; }
        try
        {
            for (auto& packet : packets)
            {
                if (packets_.size() >= 1024 || packet.Payload.size() > 1024 * 1024 - bytes_)
                { throw std::runtime_error("Room delivery backlog exceeded."); }
                bytes_ += packet.Payload.size();
                packets_.push_back(std::move(packet));
            }
        }
        catch (...) { failed_ = true; packets_.clear(); bytes_ = 0; }
    }
    std::vector<Protocol::TcpPacket> RoomPacketQueue::Drain()
    {
        std::lock_guard lock(mutex_);
        if (failed_) { throw std::runtime_error("Room delivery queue failed."); }
        std::vector<Protocol::TcpPacket> result;
        result.swap(packets_);
        bytes_ = 0;
        return result;
    }
    void RoomPacketQueue::Clear()
    {
        std::lock_guard lock(mutex_);
        packets_.clear(); bytes_ = 0;
    }
}
