#include "RespawnParameters.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        void WriteOptional(PacketWriter& writer, const std::optional<std::uint32_t>& value)
        {
            writer.WriteUInt8(value ? 0x3A : 0xBA);
            if (value) { writer.WriteUInt32(*value); }
        }
        std::optional<std::uint32_t> ReadOptional(PacketReader& reader)
        {
            const auto marker = reader.ReadUInt8();
            if (marker == 0x3A) { return reader.ReadUInt32(); }
            if (marker == 0xBA) { return std::nullopt; }
            // Native readers treat every non-0x3A marker as absent. This
            // server accepts only markers produced by the confirmed writer.
            throw std::runtime_error("Noncanonical respawn optional marker.");
        }
    }

    void RespawnParameters::WriteTo(PacketWriter& writer) const
    {
        writer.WriteUInt32(ActorObjectId);
        WriteOptional(writer, ValueAt4);
        WriteOptional(writer, ValueAt12);
    }
    RespawnParameters RespawnParameters::ReadFrom(PacketReader& reader)
    {
        RespawnParameters result;
        result.ActorObjectId = reader.ReadUInt32();
        result.ValueAt4 = ReadOptional(reader);
        result.ValueAt12 = ReadOptional(reader);
        return result;
    }
}
