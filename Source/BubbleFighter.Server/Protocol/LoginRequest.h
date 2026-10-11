#pragma once

#include "TcpPacketCodec.h"

#include <array>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02: LoginStage -> 0x0085FF00 -> 0x010C6A00 / 0x010C74A0.
    // This is a wire record. Decoding it does not authenticate its Passport.
    struct LoginRequest
    {
        static constexpr std::uint16_t MessageId = 0x0003;
        // Local support limit based on the ten-adapter loop at 0x010BBCB0.
        // The vector serializer itself does not impose this limit.
        static constexpr std::uint32_t MaxLocalIpv4Addresses = 10;

        std::u16string NexonPassport;
        std::u16string HardwareId;
        std::u16string UnknownString;
        // Octets in textual address order, e.g. {192, 0, 2, 1}.
        // The client serializes inet_addr's four memory bytes unchanged.
        std::vector<std::array<std::uint8_t, 4>> LocalIpv4Addresses;
        std::uint32_t ResourceValue = 0;
        std::uint32_t UnknownValue = 0;
        std::u16string PlatformString;

        TcpPacket Encode() const;
        static LoginRequest Decode(const TcpPacket& packet);
    };
}
