#pragma once

#include "FighterInfo.h"
#include "SessionDescriptor.h"
#include "TcpPacketCodec.h"
#include "UpdatePartyNotification.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    class PacketReader;
    class PacketWriter;
    struct SessionMemberOptionalFields
    {
        std::array<std::uint32_t, 2> ValuesAt204{};
        std::uint8_t MarkerAt212 = 0xBA;
        std::optional<std::array<std::uint32_t, 2>> OptionalAt212;
        bool operator==(const SessionMemberOptionalFields&) const = default;
    };

    // E047: reader 0x01143710. Offsets refer to the x86 client object.
    // Signed byte/short fields retain their wire bits; uint32 booleans retain
    // noncanonical values. Zero initialization is not a usable player profile.
    struct SessionMember
    {
        std::array<std::uint32_t, 3> BooleansAt0{};
        std::uint8_t ByteAt3 = 0;
        std::uint8_t SignedByteAt4 = 0;
        // E050: occupied flag is BooleansAt0[0]; +8/+12 are player ID/name.
        std::uint32_t ValueAt8 = 0;
        std::u16string TextAt12;
        // E050: second short becomes the color palette index at client +40.
        std::array<std::uint16_t, 2> SignedShortsAt36{};
        std::array<std::uint32_t, 3> BooleansAt96{};
        // E050: these and ValuesAt52/ValuesAt64/ValueAt88 feed UDP peer setup.
        std::array<std::uint32_t, 2> ValuesAt44{};
        std::vector<std::uint32_t> ValuesAt52;
        std::array<std::uint32_t, 2> ValuesAt64{};
        std::uint32_t ValueAt88 = 0;
        std::uint32_t BooleanAt94 = 0;
        std::uint32_t ValueAt100 = 0;
        std::uint32_t BooleanAt104 = 0;
        std::uint32_t ValueAt108 = 0;
        std::uint32_t BooleanAt112 = 0;
        std::u16string TextAt116;
        std::uint32_t ValueAt140 = 0;
        std::u16string TextAt144;
        std::array<std::uint32_t, 2> BooleansAt168{};
        // E051: first value is the ordinary LevelSymbolIcon level; remaining
        // values select/affect alternative icon modes and stay explicit.
        std::array<std::uint32_t, 3> ValuesAt172{};
        std::uint8_t ByteAt184 = 0;
        std::uint32_t BooleanAt185 = 0;
        std::array<std::uint32_t, 4> ValuesAt188{};
        std::uint8_t MarkerAt204 = 0xBA;
        std::optional<SessionMemberOptionalFields> OptionalAt204;
        std::uint32_t BooleanAt232 = 0;
        SocialSystemInfo Social;
        bool operator==(const SessionMember&) const = default;
    };

    struct SessionTeamGroup
    {
        std::uint8_t SignedKey = 0;
        std::vector<std::uint32_t> Values;
        bool operator==(const SessionTeamGroup&) const = default;
    };

    struct SessionTeam
    {
        std::vector<SessionTeamGroup> Groups;
        std::array<std::vector<std::uint32_t>, 2> Values;
        bool operator==(const SessionTeam&) const = default;
    };

    // Shared original record codecs used by S117, S503 and S511.
    void WriteSessionMember(PacketWriter& writer, const SessionMember& value, std::size_t& budget);
    SessionMember ReadSessionMember(PacketReader& reader, std::size_t& budget);
    void WriteSessionRecords(PacketWriter& writer, const std::vector<PartySupplementaryRecord>& records, std::size_t& budget);
    std::vector<PartySupplementaryRecord> ReadSessionRecords(PacketReader& reader, std::size_t& budget);
    void WriteSessionTeam(PacketWriter& writer, const SessionTeam& team, std::size_t& budget);
    SessionTeam ReadSessionTeam(PacketReader& reader, std::size_t& budget);

    struct SessionOptionalValueGroup
    {
        std::uint32_t Value = 0;
        std::vector<std::uint32_t> Values;
        bool operator==(const SessionOptionalValueGroup&) const = default;
    };

    struct SessionOptionalFields
    {
        std::uint32_t ValueAt0 = 0;
        std::uint64_t WideAt8 = 0;
        std::vector<std::u16string> TextsAt16;
        std::uint8_t MarkerAt28 = 0xBA;
        std::optional<SessionOptionalValueGroup> OptionalAt28;
        std::array<std::uint32_t, 5> ValuesAt48{};
        bool operator==(const SessionOptionalFields&) const = default;
    };

    // E045/E047: gspGotoRoom, also used for menu/plaza stage initialization.
    // Used by authenticated local menu/plaza initialization.
    struct GotoRoomNotification
    {
        static constexpr std::uint16_t MessageId = 0x0075;
        static constexpr std::uint8_t PresentMarker = 0x3A;
        static constexpr std::uint8_t AbsentMarker = 0xBA;
        static constexpr std::uint32_t MaxCollectionEntries = 1024;
        // Descriptor and remaining payload each have an independent 4096 budget.
        static constexpr std::uint32_t MaxTotalCollectionEntries = 4096;

        SessionDescriptor Descriptor;
        std::uint32_t ValueAt544 = 0;
        std::uint32_t ValueAt548 = 0;
        std::vector<SessionMember> Members;
        // E050: first map keyed by slot index, second map by player ID.
        std::vector<PartySupplementaryRecord> RecordsAt564;
        std::vector<PartySupplementaryRecord> RecordsAt588;
        SessionTeam Team;
        std::uint8_t MarkerAt648 = AbsentMarker;
        std::optional<SessionOptionalFields> OptionalAt648;
        // E050: local slot index, passed to context constructor 0x009D9250.
        // Do not substitute the old version's second word after the descriptor.
        std::uint32_t ValueAt728 = 0;
        std::uint32_t BooleanAt732 = 0;

        TcpPacket Encode() const;
        static GotoRoomNotification Decode(const TcpPacket& packet);
        bool operator==(const GotoRoomNotification&) const = default;
    };
}
