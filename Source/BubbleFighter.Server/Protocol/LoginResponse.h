#pragma once

#include "TcpPacketCodec.h"

#include <optional>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    struct LoginSuccessFields
    {
        // E019: copied to crash-report ACCOUNT_ID and CHAR_NAME respectively.
        std::u16string AccountIdentifier;
        std::u16string CharacterName;
        // E023: resource selection compares this with ForDao, ForDizini, etc.
        std::uint32_t CharacterType = 0;
        std::uint32_t UnknownValue = 0;
        std::uint32_t ChannelCategory = 0;
        std::uint32_t UnknownConnectionValue = 0;
        // E019: OLE DATE double, preserved as raw little-endian IEEE-754 bits.
        // The original server's time zone has not been established.
        std::uint64_t ServerTimeBits = 0;
        // E023: signed-positive values enable the client's GM branches.
        // Preserve the wire value; privilege levels have not been established.
        std::uint32_t GameMasterValue = 0;

        bool operator==(const LoginSuccessFields&) const = default;
    };

    struct LoginNationalDetails
    {
        std::uint8_t ByteValue = 0;
        std::u16string FirstString;
        std::u16string SecondString;
        // The original reader converts nonzero to true. Preserve the wire value.
        std::uint32_t BooleanValue = 0;

        bool operator==(const LoginNationalDetails&) const = default;
    };

    struct LoginNationalFields
    {
        std::uint32_t Result = 0;
        std::optional<LoginNationalDetails> Details;

        bool operator==(const LoginNationalFields&) const = default;
    };

    // BFCLIENT02: 0x0099C370 reads base response, national data, then a marker.
    // Unknown field meanings remain neutral; encoding does not approve a login.
    struct LoginResponse
    {
        static constexpr std::uint16_t MessageId = 0x0003;
        static constexpr std::uint8_t ExtensionMarker = 0x3A;

        std::uint32_t Result = 0;
        // Client object +32: included in both result branches; meaning depends on result.
        std::uint32_t IdentityValue = 0;
        std::optional<LoginSuccessFields> Success;
        LoginNationalFields National;
        // Explicit marker required by this codec, even when it is not 0x3A.
        std::uint8_t TailMarker = 0;
        std::optional<std::uint32_t> TailValue;

        TcpPacket Encode() const;
        static LoginResponse Decode(const TcpPacket& packet);
        bool operator==(const LoginResponse&) const = default;
    };
}
