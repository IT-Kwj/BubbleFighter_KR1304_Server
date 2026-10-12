#pragma once
#include "PacketReader.h"
#include "PacketWriter.h"
#include <string>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    // E067: uint32 count followed by uint32-length UTF-16LE strings.
    // These aggregate limits are local resource policies, not original limits.
    struct ItemUseArguments
    {
        static constexpr std::size_t MaxCount = 32;
        static constexpr std::size_t MaxPayloadBytes = 65536;
        static void Write(PacketWriter& writer, const std::vector<std::u16string>& values);
        static std::vector<std::u16string> Read(PacketReader& reader);
    };
}
