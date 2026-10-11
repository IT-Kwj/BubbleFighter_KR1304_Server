#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    class PacketWriter
    {
    public:
        // E011: the client's string reader handles counts up to 0x4000.
        // This server rejects larger strings instead of reproducing unsafe reads.
        static constexpr std::uint32_t MaxStringCodeUnits = 0x4000;
        void WriteUInt8(std::uint8_t value);
        void WriteUInt16(std::uint16_t value);
        void WriteUInt32(std::uint32_t value);
        void WriteUInt64(std::uint64_t value);
        void WriteUtf16(std::u16string_view value);
        std::vector<std::uint8_t> TakeBytes();

    private:
        std::vector<std::uint8_t> bytes_;
    };
}
