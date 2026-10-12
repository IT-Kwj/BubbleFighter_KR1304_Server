#pragma once
#include <cstdint>
#include <optional>

namespace BubbleFighter::Server::Protocol
{
    class PacketReader;
    class PacketWriter;

    // E080: shared 0x010C0430 writer / 0x010C04D0 reader. Optional
    // field meanings are unresolved; offsets refer to the x86 object.
    struct RespawnParameters
    {
        std::uint32_t ActorObjectId = 0;
        std::optional<std::uint32_t> ValueAt4;
        std::optional<std::uint32_t> ValueAt12;

        void WriteTo(PacketWriter& writer) const;
        static RespawnParameters ReadFrom(PacketReader& reader);
        bool operator==(const RespawnParameters&) const = default;
    };
}
