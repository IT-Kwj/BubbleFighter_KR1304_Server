#pragma once

#include "TcpPacketCodec.h"

#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R2: LobbyStage handler 0x00853E90 case 102 reads three
    // uint32 values and passes them to 0x00856EE0. Result 0 sets the client's
    // room-creator flag (0x018E566C + 0x348); nonzero values select a message.
    enum class CreateRoomResult : std::uint32_t
    {
        Created = 0,
        // "방 만들기에 실패했습니다." - also the client's default text.
        Failed = 1,
        // session/createRoomFailed.MemberNotAvailable, formatted with DetailValue.
        MemberNotAvailable = 2,
        // The selected game mode cannot be played.
        ModeUnavailable = 3,
        ClanTicketRequired = 4,
        // 5 appends text from 0x00899F10 to the code-1 message; meaning unknown.
        ReportPenaltyFriendlyOnly = 6,
        LabMembershipRequired = 7,
    };

    struct CreateRoomResponse
    {
        static constexpr std::uint16_t MessageId = 0x0066;

        // Read at 0x00853F23 but unused by 0x00856EE0; meaning unknown.
        std::uint32_t ValueAt0 = 0;
        std::uint32_t Result = 0;
        // Format argument used only by result 2 (0x008570CD -> 0x008566E0).
        std::uint32_t DetailValue = 0;

        TcpPacket Encode() const;
        static CreateRoomResponse Decode(const TcpPacket& packet);
        bool operator==(const CreateRoomResponse&) const = default;
    };
}
