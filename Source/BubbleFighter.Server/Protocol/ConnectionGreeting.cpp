#include "ConnectionGreeting.h"

namespace BubbleFighter::Server::Protocol
{
    std::array<std::uint8_t, ConnectionGreeting::Size> ConnectionGreeting::Create()
    {
        // E007: sub_10BA7D0 consumes 258 plaintext bytes; sub_99EAD0
        // accepts a little-endian word equal to 4. Remaining bytes are unused
        // on that branch, so this server fills them with zero.
        std::array<std::uint8_t, Size> greeting{};
        greeting[0] = 4;
        return greeting;
    }
}
