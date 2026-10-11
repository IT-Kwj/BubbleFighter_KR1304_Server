#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    class PacketReader
    {
    public:
        explicit PacketReader(std::span<const std::uint8_t> bytes);
        std::uint8_t ReadUInt8();
        std::uint16_t ReadUInt16();
        std::uint32_t ReadUInt32();
        std::uint64_t ReadUInt64();
        std::u16string ReadUtf16();
        void RequireEnd() const;
        std::size_t RemainingBytes() const noexcept;

    private:
        std::span<const std::uint8_t> ReadBytes(std::size_t size);
        std::span<const std::uint8_t> remaining_;
    };
}
