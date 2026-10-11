#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace BubbleFighter::Server::Protocol
{
    enum class WireDirection
    {
        ClientToServer,
        ServerToClient
    };

    class AdditiveStreamCipher
    {
    public:
        explicit AdditiveStreamCipher(WireDirection direction);

        // Use a separate instance for each connection direction. Indices persist
        // across chunks and packets; the 258-byte greeting is excluded.
        void Encode(std::span<std::uint8_t> bytes);
        void Decode(std::span<std::uint8_t> bytes);

    private:
        void Transform(std::span<std::uint8_t> bytes, bool encode);

        std::size_t firstIndex_;
        std::size_t secondIndex_;
    };
}
