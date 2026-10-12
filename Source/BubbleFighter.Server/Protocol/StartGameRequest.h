#pragma once

#include "TcpPacketCodec.h"

#include <array>
#include <cstdint>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R21: block from 0x00AADD50 (map rules), written field by
    // field by 0x010D26C0. Opaque here; 0x009A0880 overrides one value with a
    // time limit for modes 8/28/43 (180000) and 32 (600000).
    struct StartGameRules
    {
        std::array<std::uint32_t, 13> Values{};
        std::uint8_t ByteAt52 = 0;
        bool operator==(const StartGameRules&) const = default;
    };

    // RoomCreation R21: C503, the start request of ordinary rooms. Start
    // button 0x00970590 -> 0x0096DD20 -> 0x009A0880 -> writer 0x009D7570.
    // Round-battle rooms (option 0x1, mode != 19) send C504 instead.
    struct StartGameRequest
    {
        static constexpr std::uint16_t MessageId = 0x01F7;

        // Byte argument of 0x009A0880 (0 from the start button), zero-extended.
        std::uint32_t Variant = 0;
        // 0x010E48A0: map resource resolved from the room's map selection.
        std::u16string MapResource;
        // 0x010E49C0: a value read from that resource.
        std::u16string MapAttribute;
        // Copy of room context +184, the selected map key.
        std::u16string MapKey;
        // 0x0041B060(0, 1, 0); meaning unknown.
        std::uint32_t ValueAt76 = 0;
        StartGameRules Rules;
        // Cleared by 0x0099B890; 0x009A0880 can only set bit 0.
        std::uint32_t Flags = 0;

        TcpPacket Encode() const;
        static StartGameRequest Decode(const TcpPacket& packet);
        bool operator==(const StartGameRequest&) const = default;
    };
}
