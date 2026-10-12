#pragma once
#include "RoomMatchSettlement.h"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace BubbleFighter::Server::Features::Rooms
{
    // A frozen match belongs to the settlement service, independently of room
    // membership, room identity reuse and the lifetime of its last connection.
    class RoomSettlementQueue
    {
    public:
        using Time = std::chrono::steady_clock::time_point;
        static constexpr auto RetryDelay = std::chrono::seconds{2};
        class Job
        {
        public:
            const RoomMatchRequest Request;
            std::optional<RoomMatchResult> Result() const;
        private:
            friend class RoomSettlementQueue;
            explicit Job(RoomMatchRequest request) : Request(std::move(request)) {}
            mutable std::mutex mutex_;
            std::optional<RoomMatchResult> result_;
            Time retryAt_{};
            bool running_ = false, delayed_ = false;
        };
        explicit RoomSettlementQueue(RoomMatchSettler settler);
        std::shared_ptr<Job> Enqueue(RoomMatchRequest request);
        void TrySettle(const std::shared_ptr<Job>& job, Time now);
        // Provider calls happen without the queue or room-table lock. Another
        // thread may submit/retrieve work while the database is unavailable.
        void ProcessPending(Time now);
        std::size_t PendingCount() const;
    private:
        RoomMatchSettler settler_;
        mutable std::mutex mutex_;
        std::map<std::array<std::uint8_t, 16>, std::shared_ptr<Job>> jobs_;
    };
}
