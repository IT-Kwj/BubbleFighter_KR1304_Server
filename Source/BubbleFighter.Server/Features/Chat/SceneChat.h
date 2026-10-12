#pragma once
#include "Protocol/ChatRequest.h"
#include "Protocol/ChatNotification.h"
#include "Features/Characters/CharacterStore.h"
#include "Features/Inventory/InventorySlot.h"
#include <chrono>
#include <deque>
#include <string_view>
namespace BubbleFighter::Server::Features::Chat
{
    // Local resource limits; not recovered original server moderation rules.
    class SceneChat
    {
    public:
        using Time = std::chrono::steady_clock::time_point;
        // Constructed after authentication and resource/ownership checks. This
        // is an internal message, never a decoded client authorization claim.
        struct Message
        {
            std::uint32_t Channel = 0;
            std::u16string Text;
            std::u16string Motion;
        };
        static bool ValidText(std::u16string_view text, std::size_t maximum = 256) noexcept;
        static bool NeedsInventory(std::u16string_view text) noexcept;
        static Message Prepare(const Protocol::ChatRequest& request, const Characters::CharacterRecord& character,
            const Inventory::CharacterInventory* inventory = nullptr, std::uint64_t observedAtUnixSeconds = 0);
        static Protocol::ChatNotification Create(std::int16_t slot, std::u16string_view trustedName,
            const Message& message);
        static Protocol::TcpPacket Notice(std::u16string_view text);
        bool Admit(Time now);
    private:
        std::deque<Time> sent_;
    };
}
