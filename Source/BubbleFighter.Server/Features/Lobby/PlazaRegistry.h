#pragma once
#include "Features/Scenes/SceneRoster.h"
#include "Features/Rooms/RoomPacketQueue.h"
#include "Protocol/PlazaMovement.h"
#include "Features/Chat/SceneChat.h"
#include <map>
#include <mutex>

namespace BubbleFighter::Server::Features::Lobby
{
    class PlazaRegistry;
    class PlazaLease
    {
    public:
        PlazaLease() = default;
        PlazaLease(PlazaLease&& other) noexcept;
        PlazaLease& operator=(PlazaLease&& other) noexcept;
        PlazaLease(const PlazaLease&) = delete;
        PlazaLease& operator=(const PlazaLease&) = delete;
        ~PlazaLease();
        explicit operator bool() const noexcept;
        void Release() noexcept;
        void Move(const Protocol::PlazaMovementRequest& movement);
        void Refresh(Scenes::SceneSlot player, bool sendSnapshot);
        void Chat(const Features::Chat::SceneChat::Message& request);
    private:
        friend class PlazaRegistry;
        PlazaLease(std::shared_ptr<PlazaRegistry> registry, std::uint16_t scene, std::uint64_t account) noexcept;
        std::shared_ptr<PlazaRegistry> registry_;
        std::uint16_t scene_ = 0;
        std::uint64_t account_ = 0;
    };

    // One registry for the server's current local channel. First non-full
    // plaza, lowest vacant slot; capacity 16 is the user's local policy.
    // Ordered delivery under the lock uses enqueue-only, nonblocking sinks.
    class PlazaRegistry : public std::enable_shared_from_this<PlazaRegistry>
    {
    public:
        static constexpr std::size_t Capacity = 16;
        PlazaLease Enter(Protocol::GotoRoomNotification envelope, Scenes::SceneSlot player,
            std::uint64_t account, Rooms::RoomPacketSink sink);
    private:
        friend class PlazaLease;
        struct Member
        {
            std::uint32_t Slot = 0;
            Scenes::SceneSlot Player;
            Rooms::RoomPacketSink Sink;
            std::optional<Protocol::PlazaMovementRequest> Movement;
        };
        struct Scene
        {
            Protocol::GotoRoomNotification Envelope;
            std::map<std::uint64_t, Member> Members;
        };
        static Protocol::GotoRoomNotification Snapshot(const Scene& scene, std::uint64_t account);
        static Protocol::TcpPacket Update(const Protocol::GotoRoomNotification& snapshot);
        static void Deliver(const Member& member, std::vector<Protocol::TcpPacket> packets) noexcept;
        void Leave(std::uint16_t scene, std::uint64_t account) noexcept;
        void Move(std::uint16_t scene, std::uint64_t account, const Protocol::PlazaMovementRequest& movement);
        void Refresh(std::uint16_t scene, std::uint64_t account, Scenes::SceneSlot player, bool sendSnapshot);
        void Chat(std::uint16_t scene, std::uint64_t account, const Features::Chat::SceneChat::Message& request);
        std::mutex mutex_;
        std::map<std::uint16_t, Scene> scenes_;
    };
}
