#pragma once
#include "Features/Characters/CharacterStatistics.h"
#include "ResultAwardRules.h"
#include "Protocol/TcpPacketCodec.h"
#include <array>
#include <filesystem>
#include <istream>

namespace BubbleFighter::Server::Features::Rooms
{
    // User-authorized LOCAL policy. These numbers are not Nexon's reward table.
    struct LocalMatchRules
    {
        std::uint32_t ExperiencePerLevel = 1000, MaximumLevel = 260;
        std::uint32_t CompletionExperience = 100, CompletionLucci = 50;
        std::uint32_t WinExperience = 25, WinLucci = 10;
        std::uint32_t PangExperience = 10, PangLucci = 5;
        std::uint32_t RescueExperience = 5, RescueLucci = 2;
        ResultAwardRules Awards;
        static LocalMatchRules Read(std::istream& input);
        static LocalMatchRules Load(const std::filesystem::path& path);
        void Validate() const;
        Characters::CharacterStatistics Apply(const Characters::CharacterStatistics& before,
            const std::array<std::uint16_t, 4>& scores, bool won) const;
        std::array<std::uint32_t, 2> ExperienceRange(std::uint32_t experience) const;
        // S105 range bindings recovered in RoomSnapshot; all app delivery paths
        // use this same policy, including menu, purchase and match settlement.
        void BindProfiles(std::vector<Protocol::TcpPacket>& packets) const;
        bool operator==(const LocalMatchRules&) const = default;
    };
}
