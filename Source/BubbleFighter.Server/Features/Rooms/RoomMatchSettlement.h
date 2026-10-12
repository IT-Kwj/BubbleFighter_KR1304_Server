#pragma once
#include "LocalMatchRules.h"
#include "Features/Characters/CharacterStore.h"
#include "Protocol/GameScoreNotification.h"
#include <functional>

namespace BubbleFighter::Server::Features::Rooms
{
    struct RoomMatchMember
    {
        std::uint32_t Slot = 0;
        std::uint64_t AccountId = 0, CharacterId = 0;
        std::uint8_t Team = 0;
        // Bubble, pang, help, death: accepted server transitions only.
        std::array<std::uint16_t, 4> Scores{};
        bool operator==(const RoomMatchMember&) const = default;
    };
    struct RoomMatchRequest
    {
        std::array<std::uint8_t, 16> MatchId{};
        LocalMatchRules Rules;
        std::uint32_t DurationMilliseconds = 0;
        std::array<std::uint16_t, 2> TeamScores{};
        std::vector<RoomMatchMember> Members;
        std::uint32_t FormatVersion = 2; // V1 receipts retain their original no-awards presentation.
        void Validate() const;
        std::uint8_t Winner() const noexcept;
        // Versioned LOCAL durable request, not a game protocol message.
        std::vector<std::uint8_t> Encode() const;
        static RoomMatchRequest Decode(std::span<const std::uint8_t> data);
    };
    struct RoomMatchReward
    {
        std::uint32_t Slot = 0;
        Characters::CharacterRecord Character;
        Characters::CharacterStatistics Before;
        bool operator==(const RoomMatchReward&) const = default;
    };
    struct RoomMatchResult
    {
        std::vector<RoomMatchReward> Rewards;
        void Validate(const RoomMatchRequest& request) const;
        std::vector<std::uint8_t> Encode() const;
        static RoomMatchResult Decode(std::span<const std::uint8_t> data);
        Protocol::GameScoreNotification Scores(const RoomMatchRequest& request) const;
        std::vector<std::pair<std::uint32_t, Protocol::TcpPacket>> Profiles(const RoomMatchRequest& request) const;
        bool operator==(const RoomMatchResult&) const = default;
    };
    using RoomMatchSettler = std::function<RoomMatchResult(const RoomMatchRequest&)>;
    std::array<std::uint8_t, 16> CreateRoomMatchId();
}
