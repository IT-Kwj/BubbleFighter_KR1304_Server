#pragma once

#include "TcpPacketCodec.h"

#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R17: S103, LobbyStage 0x00853E90 case 0x67. Zero shows
    // nothing; other codes select text in 0x00857990 (codes 11 and 20+ have no
    // entry). Only the codes this server sends are named.
    enum class JoinRoomResult : std::uint32_t
    {
        Joined = 0,
        // "이미 게임이 시작된 방입니다."
        GameStarted = 1,
        // "방 인원이 꽉 찼어요."
        RoomFull = 3,
        // "방 비밀 번호가 틀렸습니다."
        WrongPassword = 4,
        // "조건에 맞는 방이 없습니다."
        NoMatchingRoom = 5,
    };

    struct JoinRoomResponse
    {
        static constexpr std::uint16_t MessageId = 0x0067;

        std::uint32_t Result = 0;
        // Only the level-limit text (code 19, "%d") formats a value.
        std::uint32_t DetailValue = 0;

        TcpPacket Encode() const;
        static JoinRoomResponse Decode(const TcpPacket& packet);
        bool operator==(const JoinRoomResponse&) const = default;
    };
}
