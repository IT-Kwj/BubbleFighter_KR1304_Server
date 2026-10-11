#pragma once
#include "TcpPacketCodec.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    class PacketReader;
    class PacketWriter;
    struct PartyMember
    {
        std::uint32_t IdentityValue = 0;
        std::u16string CharacterName;
        // E041: client offsets, not wire offsets; full semantics unknown.
        std::array<std::uint32_t, 4> ValuesAt32{};
        bool operator==(const PartyMember&) const = default;
    };

    struct PartySupplementaryDetail
    {
        std::array<std::uint32_t, 2> Values{};
        std::vector<std::uint32_t> Entries;
        std::uint32_t FinalValue = 0;
        std::u16string Text;
        bool operator==(const PartySupplementaryDetail&) const = default;
    };

    struct PartySupplementaryRecord
    {
        std::uint32_t Key = 0;
        std::uint32_t Value = 0;
        std::array<std::vector<std::uint32_t>, 3> Groups;
        std::vector<std::uint8_t> Bytes;
        std::vector<PartySupplementaryDetail> Details;
        // Shared with S117 (E047); includes Key. The caller owns a cumulative
        // collection budget so nested records cannot reset allocation limits.
        void Write(PacketWriter& writer, std::size_t& budget) const;
        static PartySupplementaryRecord Read(PacketReader& reader, std::size_t& budget);
        bool operator==(const PartySupplementaryRecord&) const = default;
    };

    // E041: gspUpdateParty, including the trailing supplementary map.
    // Preserve wire order. These local resource limits are not original rules.
    struct UpdatePartyNotification
    {
        static constexpr std::uint16_t MessageId = 0x00C9;
        static constexpr std::uint32_t MaxCollectionEntries = 1024;
        static constexpr std::uint32_t MaxTotalEntries = 4096;
        std::uint32_t State = 0;
        std::vector<PartyMember> Members;
        std::vector<PartySupplementaryRecord> SupplementaryRecords;
        TcpPacket Encode() const;
        static UpdatePartyNotification Decode(const TcpPacket& packet);
        bool operator==(const UpdatePartyNotification&) const = default;
    };
}
