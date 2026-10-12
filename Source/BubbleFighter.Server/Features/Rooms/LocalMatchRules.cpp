#include "LocalMatchRules.h"
#include "Protocol/UserStatNotification.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Rooms
{
    namespace
    {
        constexpr auto Max = Characters::CharacterStatistics::MaxValue;
        std::uint32_t Add(std::uint32_t value, std::uint64_t gain)
        { return static_cast<std::uint32_t>(std::min<std::uint64_t>(Max, value + gain)); }
    }

    LocalMatchRules LocalMatchRules::Read(std::istream& input)
    {
        LocalMatchRules rules;
        std::map<std::string, std::uint32_t*> fields{
            {"ExperiencePerLevel", &rules.ExperiencePerLevel}, {"MaximumLevel", &rules.MaximumLevel},
            {"CompletionExperience", &rules.CompletionExperience}, {"CompletionLucci", &rules.CompletionLucci},
            {"WinExperience", &rules.WinExperience}, {"WinLucci", &rules.WinLucci},
            {"PangExperience", &rules.PangExperience}, {"PangLucci", &rules.PangLucci},
            {"RescueExperience", &rules.RescueExperience}, {"RescueLucci", &rules.RescueLucci}};
        constexpr std::array grades{"SSS","SS","S","A","B","C","D","E"};
        constexpr std::array categories{"Bubble","Pang","Rescue"};
        constexpr std::array tiers{"Beginner","Expert","Master"};
        for (std::size_t i = 0; i < grades.size(); ++i)
        { fields.emplace(std::string("Grade") + grades[i] + "Minimum", &rules.Awards.GradeMinimums[i]); }
        for (std::size_t i = 0; i < categories.size(); ++i)
        {
            for (std::size_t j = 0; j < tiers.size(); ++j)
            { fields.emplace(std::string("Medal") + categories[i] + tiers[j] + "Minimum", &rules.Awards.MedalMinimums[i][j]); }
        }
        std::set<std::string> seen;
        std::string line;
        std::size_t bytes = 0;
        while (std::getline(input, line))
        {
            bytes += line.size() + 1;
            if (bytes > 16384) { throw std::invalid_argument("Local match rules file is too large."); }
            if (!line.empty() && line.back() == '\r') { line.pop_back(); }
            if (line.empty() || line.front() == '#') { continue; }
            const auto equals = line.find('=');
            const auto name = line.substr(0, equals);
            const auto field = fields.find(name);
            if (equals == std::string::npos || field == fields.end() || !seen.insert(name).second)
            { throw std::invalid_argument("Unknown or duplicate local match rule."); }
            const auto value = std::string_view(line).substr(equals + 1);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), *field->second);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            { throw std::invalid_argument("Local match rules require unsigned decimal values."); }
        }
        if (input.bad()) { throw std::runtime_error("Cannot read local match rules."); }
        rules.Validate();
        return rules;
    }

    LocalMatchRules LocalMatchRules::Load(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file) { throw std::runtime_error("Cannot open local match rules."); }
        return Read(file);
    }

    void LocalMatchRules::Validate() const
    {
        Awards.Validate();
        if (ExperiencePerLevel == 0 || ExperiencePerLevel > Max || MaximumLevel == 0 || MaximumLevel > 260)
        { throw std::invalid_argument("Unsupported local experience progression."); }
        for (auto value : {CompletionExperience, CompletionLucci, WinExperience, WinLucci,
            PangExperience, PangLucci, RescueExperience, RescueLucci})
        { if (value > 1000000) { throw std::invalid_argument("Local reward exceeds the configured limit."); } }
    }

    Characters::CharacterStatistics LocalMatchRules::Apply(const Characters::CharacterStatistics& before,
        const std::array<std::uint16_t, 4>& scores, bool won) const
    {
        Validate();
        if (!before.IsValid()) { throw std::invalid_argument("Invalid reward recipient statistics."); }
        auto after = before;
        after.Experience = Add(before.Experience, std::uint64_t{CompletionExperience} + (won ? WinExperience : 0) +
            std::uint64_t{scores[1]} * PangExperience + std::uint64_t{scores[2]} * RescueExperience);
        after.Lucci = Add(before.Lucci, std::uint64_t{CompletionLucci} + (won ? WinLucci : 0) +
            std::uint64_t{scores[1]} * PangLucci + std::uint64_t{scores[2]} * RescueLucci);
        // Preserve legacy levels even when they do not match cumulative XP.
        // Only thresholds crossed by this award add levels; no retroactive reset.
        if (before.Level < MaximumLevel)
        {
            const auto crossed = after.Experience / ExperiencePerLevel - before.Experience / ExperiencePerLevel;
            after.Level = static_cast<std::uint32_t>(std::min<std::uint64_t>(MaximumLevel, std::uint64_t{before.Level} + crossed));
        }
        return after;
    }

    std::array<std::uint32_t, 2> LocalMatchRules::ExperienceRange(std::uint32_t experience) const
    {
        Validate();
        if (experience > Max) { throw std::invalid_argument("Experience outside client signed range."); }
        const auto base = experience / ExperiencePerLevel * ExperiencePerLevel;
        return {base, Add(base, ExperiencePerLevel)};
    }

    void LocalMatchRules::BindProfiles(std::vector<Protocol::TcpPacket>& packets) const
    {
        for (auto& packet : packets)
        {
            if (packet.MessageId != Protocol::UserStatNotification::MessageId) { continue; }
            auto profile = Protocol::UserStatNotification::Decode(packet);
            const auto range = ExperienceRange(profile.Values[1]);
            profile.Values[2] = range[0]; profile.Values[3] = range[1];
            packet = profile.Encode();
        }
    }
}
