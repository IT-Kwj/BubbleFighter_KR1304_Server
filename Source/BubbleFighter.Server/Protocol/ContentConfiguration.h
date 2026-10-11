#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    struct ContentConfiguration
    {
        static constexpr std::size_t Category = 14;
        static constexpr std::size_t MaxEntries = 1024;
        std::u16string CreateUserUiPath;
        std::vector<std::uint32_t> CharacterTypes;
        std::map<std::uint32_t, std::uint32_t> PreviewOverrides;
        // Read as binary32 and added to a client date value; units unresolved.
        std::uint32_t TimeOffsetBitsAt52 = 0;
        std::uint32_t Item103385GaugeMaximum = 0;
        std::vector<std::uint8_t> Encode() const;
    };
}
