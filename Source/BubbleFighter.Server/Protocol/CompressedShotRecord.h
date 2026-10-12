#pragma once
#include <array>
#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    class PacketReader;
    class PacketWriter;

    // E086: 32 wire bytes. At offsets refer to the client's padded record.
    // Keep binary16 bits verbatim, including signed zero and NaN payloads.
    struct CompressedShotRecord
    {
        std::uint32_t ActorObjectId = 0;
        std::uint8_t WeaponIndex = 0;
        std::array<std::uint16_t, 3> Position{};
        std::array<std::uint16_t, 3> VectorAt12{};
        std::uint32_t ValueAt20 = 0;
        std::uint8_t ByteAt24 = 0;
        std::uint32_t ShotSequence = 0;
        std::uint32_t PackedTimeAt32 = 0;
        std::uint16_t ShortAt36 = 0;
        void Encode(PacketWriter& writer) const;
        static CompressedShotRecord Decode(PacketReader& reader);
        bool HasFiniteCoordinates() const noexcept;
        bool operator==(const CompressedShotRecord&) const = default;
    };
}
