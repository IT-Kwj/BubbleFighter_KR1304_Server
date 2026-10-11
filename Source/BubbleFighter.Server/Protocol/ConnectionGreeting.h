#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    class ConnectionGreeting
    {
    public:
        static constexpr std::size_t Size = 258;
        static std::array<std::uint8_t, Size> Create();
    };
}
