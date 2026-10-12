#pragma once
#include "MySqlConnection.h"
#include "Features/Rooms/RoomMatchSettlement.h"

namespace BubbleFighter::Server::Persistence
{
    class MySqlMatchStore
    {
    public:
        explicit MySqlMatchStore(const std::filesystem::path& configuration);
        // Separate durable submission: an award transaction failure does not
        // erase the original participants, scores, policy or unique identity.
        void SavePending(const Features::Rooms::RoomMatchRequest& request);
        std::vector<Features::Rooms::RoomMatchRequest> LoadPending(const std::array<std::uint8_t, 16>& after = {});
        Features::Rooms::RoomMatchResult Settle(const Features::Rooms::RoomMatchRequest& request);
    private:
        MySqlConnection connection_;
    };
}
