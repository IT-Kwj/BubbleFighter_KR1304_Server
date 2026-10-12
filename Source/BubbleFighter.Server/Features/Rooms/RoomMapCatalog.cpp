#include "RoomMapCatalog.h"
#include "RoomMapCatalogData.h"
#include <algorithm>

namespace BubbleFighter::Server::Features::Rooms
{
    std::span<const RoomMapDefinition> RoomMapCatalog::Maps() { return RoomMapDefinitions; }
    const RoomMapDefinition* RoomMapCatalog::Match(const Protocol::StartGameRequest& request)
    {
        if (request.Variant != 0 || request.Flags != 0) { return nullptr; }
        const auto maps = Maps();
        const auto found = std::find_if(maps.begin(), maps.end(), [&](const auto& map)
        {
            return request.MapResource == map.Directory && request.MapAttribute == map.Name &&
                (request.MapKey == map.Key || request.MapKey == u"Random") &&
                request.ValueAt76 == map.Difficulty && request.Rules == map.Rules;
        });
        return found == maps.end() ? nullptr : &*found;
    }
}
