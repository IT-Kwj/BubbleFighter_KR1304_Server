#include "CompressedShotRecord.h"
#include "PacketReader.h"
#include "PacketWriter.h"

namespace BubbleFighter::Server::Protocol
{
    void CompressedShotRecord::Encode(PacketWriter& writer) const
    {
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt8(WeaponIndex);
        for (const auto value : Position) { writer.WriteUInt16(value); }
        for (const auto value : VectorAt12) { writer.WriteUInt16(value); }
        writer.WriteUInt32(ValueAt20);
        writer.WriteUInt8(ByteAt24);
        writer.WriteUInt32(ShotSequence);
        writer.WriteUInt32(PackedTimeAt32);
        writer.WriteUInt16(ShortAt36);
    }

    CompressedShotRecord CompressedShotRecord::Decode(PacketReader& reader)
    {
        CompressedShotRecord value;
        value.ActorObjectId = reader.ReadUInt32();
        value.WeaponIndex = reader.ReadUInt8();
        for (auto& coordinate : value.Position) { coordinate = reader.ReadUInt16(); }
        for (auto& coordinate : value.VectorAt12) { coordinate = reader.ReadUInt16(); }
        value.ValueAt20 = reader.ReadUInt32();
        value.ByteAt24 = reader.ReadUInt8();
        value.ShotSequence = reader.ReadUInt32();
        value.PackedTimeAt32 = reader.ReadUInt32();
        value.ShortAt36 = reader.ReadUInt16();
        return value;
    }

    bool CompressedShotRecord::HasFiniteCoordinates() const noexcept
    {
        // Native FA5360 decodes these as IEEE binary16. All-ones exponent
        // produces infinity/NaN; the codec preserves it, gameplay rejects it.
        for (const auto value : Position) { if ((value & 0x7C00U) == 0x7C00U) { return false; } }
        for (const auto value : VectorAt12) { if ((value & 0x7C00U) == 0x7C00U) { return false; } }
        return true;
    }
}
