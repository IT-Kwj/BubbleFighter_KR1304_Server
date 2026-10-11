#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    // Emits standard zlib with stored DEFLATE blocks. No compression dependency
    // is needed for bounded server-owned configuration/inventory records.
    class ZlibStoredStream
    {
    public:
        static constexpr std::size_t MaxInputBytes = 256 * 1024;
        static std::vector<std::uint8_t> Encode(std::span<const std::uint8_t> bytes);
        static std::uint32_t Adler32(std::span<const std::uint8_t> bytes, std::uint32_t seed);
    };
}
