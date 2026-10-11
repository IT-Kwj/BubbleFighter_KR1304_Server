#include "ContentConfiguration.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    std::vector<std::uint8_t> ContentConfiguration::Encode() const
    {
        if (CharacterTypes.size() > MaxEntries || PreviewOverrides.size() > MaxEntries)
        { throw std::invalid_argument("Content configuration exceeds local entry limit."); }
        PacketWriter writer;
        writer.WriteUtf16(CreateUserUiPath);
        writer.WriteUInt32(static_cast<std::uint32_t>(CharacterTypes.size()));
        for (const auto type : CharacterTypes) { writer.WriteUInt32(type); }
        writer.WriteUInt32(static_cast<std::uint32_t>(PreviewOverrides.size()));
        for (const auto& [type, value] : PreviewOverrides) { writer.WriteUInt32(type); writer.WriteUInt32(value); }
        writer.WriteUInt32(TimeOffsetBitsAt52);
        writer.WriteUInt32(Item103385GaugeMaximum);
        return writer.TakeBytes();
    }
}
