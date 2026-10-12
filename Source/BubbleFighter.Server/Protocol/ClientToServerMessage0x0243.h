#pragma once
#include "TcpPacketCodec.h"
#include <array>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // E079: 0x006DA5D0 -> 0x010D14C0, after map loading. Offsets are
    // x86 object offsets, not wire offsets. Resource semantics remain partial.
    struct ClientToServerMessage0x0243
    {
        static constexpr std::uint16_t MessageId = 579;
        using Triple = std::array<std::uint32_t, 3>; // Preserve raw float bits.
        std::uint32_t ValueAt0 = 0;
        std::vector<Triple> TriplesAt4;
        std::vector<std::uint32_t> ValuesAt16;
        std::vector<std::uint8_t> BytesAt28;
        std::vector<std::uint32_t> ValuesAt40, ValuesAt52, ValuesAt64;
        std::vector<Triple> TriplesAt76;
        std::vector<std::uint32_t> ValuesAt88, ValuesAt100, ValuesAt112;
        std::vector<Triple> TriplesAt124;
        std::vector<std::uint32_t> ValuesAt136, ValuesAt148;
        std::vector<std::u16string> StringsAt160;

        TcpPacket Encode() const;
        static ClientToServerMessage0x0243 Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x0243&) const = default;
    };
}
