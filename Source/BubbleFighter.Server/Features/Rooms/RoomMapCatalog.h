#pragma once
#include "Protocol/StartGameRequest.h"
#include <span>
#include <string_view>

namespace BubbleFighter::Server::Features::Rooms
{
    struct RoomMapDefinition
    {
        std::u16string_view Key, Directory, Name;
        std::uint32_t Difficulty;
        Protocol::StartGameRules Rules;
    };
    class RoomMapCatalog
    {
    public:
        static std::span<const RoomMapDefinition> Maps();
        static const RoomMapDefinition* Match(const Protocol::StartGameRequest& request);
    };
}
