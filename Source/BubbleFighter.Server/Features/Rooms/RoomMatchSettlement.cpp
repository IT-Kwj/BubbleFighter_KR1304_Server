#include "RoomMatchSettlement.h"
#include "Features/Characters/CharacterProfile.h"
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Rooms
{
    namespace
    {
        void WriteStatistics(Protocol::PacketWriter& writer, const Characters::CharacterStatistics& s)
        { writer.WriteUInt32(s.Level); writer.WriteUInt32(s.Experience); writer.WriteUInt32(s.Lucci); writer.WriteUInt32(s.Cash); }
        Characters::CharacterStatistics ReadStatistics(Protocol::PacketReader& reader)
        { return {reader.ReadUInt32(), reader.ReadUInt32(), reader.ReadUInt32(), reader.ReadUInt32()}; }
        template <std::size_t N>
        void Put(const std::array<std::size_t, N>& offsets, std::array<std::uint32_t, N>& values,
            std::size_t offset, std::uint32_t value)
        {
            const auto at = std::find(offsets.begin(), offsets.end(), offset);
            if (at == offsets.end()) { throw std::logic_error("Unknown score field binding."); }
            values[static_cast<std::size_t>(at - offsets.begin())] = value;
        }
    }

    std::array<std::uint8_t, 16> CreateRoomMatchId()
    {
        std::array<std::uint8_t, 16> value{};
        if (BCryptGenRandom(nullptr, value.data(), static_cast<ULONG>(value.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        { throw std::runtime_error("Cannot create a unique local match identity."); }
        return value;
    }

    void RoomMatchRequest::Validate() const
    {
        Rules.Validate();
        if ((FormatVersion != 1 && FormatVersion != 2) || (FormatVersion == 1 && Rules.Awards != ResultAwardRules{}))
        { throw std::invalid_argument("Unsupported frozen result award version or legacy rule change."); }
        if (std::all_of(MatchId.begin(), MatchId.end(), [](auto v) { return v == 0; }) ||
            Members.empty() || Members.size() > 16 || DurationMilliseconds == 0)
        { throw std::invalid_argument("Invalid local match settlement."); }
        std::set<std::uint64_t> accounts, characters;
        std::optional<std::uint32_t> previous;
        for (const auto& member : Members)
        {
            if (member.Slot >= 16 || (previous && *previous >= member.Slot) || member.AccountId == 0 ||
                member.CharacterId == 0 || member.CharacterId > std::numeric_limits<std::uint32_t>::max() ||
                (member.Team != 1 && member.Team != 2) || !accounts.insert(member.AccountId).second ||
                !characters.insert(member.CharacterId).second)
            { throw std::invalid_argument("Invalid or duplicate match participant."); }
            previous = member.Slot;
        }
    }

    std::uint8_t RoomMatchRequest::Winner() const noexcept
    { return TeamScores[0] == TeamScores[1] ? 0 : (TeamScores[0] > TeamScores[1] ? 1 : 2); }

    std::vector<std::uint8_t> RoomMatchRequest::Encode() const
    {
        Validate();
        Protocol::PacketWriter writer;
        writer.WriteUInt32(FormatVersion); // Local durable format, unrelated to client version.
        for (auto value : MatchId) { writer.WriteUInt8(value); }
        for (auto value : {Rules.ExperiencePerLevel, Rules.MaximumLevel, Rules.CompletionExperience, Rules.CompletionLucci,
            Rules.WinExperience, Rules.WinLucci, Rules.PangExperience, Rules.PangLucci, Rules.RescueExperience, Rules.RescueLucci})
        { writer.WriteUInt32(value); }
        if (FormatVersion >= 2)
        {
            for (const auto value : Rules.Awards.GradeMinimums) { writer.WriteUInt32(value); }
            for (const auto& row : Rules.Awards.MedalMinimums)
            { for (const auto value : row) { writer.WriteUInt32(value); } }
        }
        writer.WriteUInt32(DurationMilliseconds);
        for (auto value : TeamScores) { writer.WriteUInt16(value); }
        writer.WriteUInt32(static_cast<std::uint32_t>(Members.size()));
        for (const auto& member : Members)
        {
            writer.WriteUInt32(member.Slot); writer.WriteUInt64(member.AccountId); writer.WriteUInt64(member.CharacterId);
            writer.WriteUInt8(member.Team);
            for (auto value : member.Scores) { writer.WriteUInt16(value); }
        }
        return writer.TakeBytes();
    }

    RoomMatchRequest RoomMatchRequest::Decode(std::span<const std::uint8_t> data)
    {
        if (data.size() > 4096) { throw std::invalid_argument("Oversized frozen match request."); }
        Protocol::PacketReader reader(data);
        RoomMatchRequest result;
        result.FormatVersion = reader.ReadUInt32();
        if (result.FormatVersion != 1 && result.FormatVersion != 2)
        { throw std::invalid_argument("Unknown frozen match version."); }
        for (auto& value : result.MatchId) { value = reader.ReadUInt8(); }
        auto& r = result.Rules;
        for (auto* value : {&r.ExperiencePerLevel, &r.MaximumLevel, &r.CompletionExperience, &r.CompletionLucci,
            &r.WinExperience, &r.WinLucci, &r.PangExperience, &r.PangLucci, &r.RescueExperience, &r.RescueLucci})
        { *value = reader.ReadUInt32(); }
        if (result.FormatVersion >= 2)
        {
            for (auto& value : r.Awards.GradeMinimums) { value = reader.ReadUInt32(); }
            for (auto& row : r.Awards.MedalMinimums)
            { for (auto& value : row) { value = reader.ReadUInt32(); } }
        }
        result.DurationMilliseconds = reader.ReadUInt32();
        for (auto& value : result.TeamScores) { value = reader.ReadUInt16(); }
        const auto count = reader.ReadUInt32();
        if (count == 0 || count > 16 || reader.RemainingBytes() != count * 29)
        { throw std::invalid_argument("Invalid frozen match member count."); }
        for (std::uint32_t i = 0; i < count; ++i)
        {
            RoomMatchMember member;
            member.Slot = reader.ReadUInt32(); member.AccountId = reader.ReadUInt64(); member.CharacterId = reader.ReadUInt64();
            member.Team = reader.ReadUInt8();
            for (auto& value : member.Scores) { value = reader.ReadUInt16(); }
            result.Members.push_back(member);
        }
        reader.RequireEnd(); result.Validate();
        return result;
    }

    void RoomMatchResult::Validate(const RoomMatchRequest& request) const
    {
        request.Validate();
        if (Rewards.size() != request.Members.size()) { throw std::invalid_argument("Incomplete match receipt."); }
        for (std::size_t i = 0; i < Rewards.size(); ++i)
        {
            const auto& reward = Rewards[i]; const auto& member = request.Members[i];
            if (reward.Slot != member.Slot || reward.Character.CharacterId != member.CharacterId ||
                reward.Character.AccountId != member.AccountId || reward.Character.Statistics !=
                request.Rules.Apply(reward.Before, member.Scores, member.Team == request.Winner()))
            { throw std::invalid_argument("Match receipt does not match trusted award inputs."); }
            Characters::CharacterProfile::BindKnownFields({}, reward.Character, member.AccountId);
        }
    }

    std::vector<std::uint8_t> RoomMatchResult::Encode() const
    {
        if (Rewards.empty() || Rewards.size() > 16) { throw std::invalid_argument("Invalid local receipt size."); }
        Protocol::PacketWriter writer;
        writer.WriteUInt32(1); writer.WriteUInt32(static_cast<std::uint32_t>(Rewards.size()));
        for (const auto& reward : Rewards)
        {
            const auto& c = reward.Character;
            writer.WriteUInt32(reward.Slot); writer.WriteUInt64(c.CharacterId); writer.WriteUInt64(c.AccountId);
            writer.WriteUtf16(c.CharacterName); writer.WriteUInt32(c.CharacterType); writer.WriteUInt32(c.ColorIndex);
            WriteStatistics(writer, reward.Before); WriteStatistics(writer, c.Statistics);
        }
        return writer.TakeBytes();
    }

    RoomMatchResult RoomMatchResult::Decode(std::span<const std::uint8_t> data)
    {
        if (data.size() > 4096) { throw std::invalid_argument("Oversized local match receipt."); }
        Protocol::PacketReader reader(data);
        if (reader.ReadUInt32() != 1) { throw std::invalid_argument("Unknown local match receipt version."); }
        const auto count = reader.ReadUInt32();
        if (count == 0 || count > 16) { throw std::invalid_argument("Invalid local receipt count."); }
        RoomMatchResult result;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            RoomMatchReward reward;
            reward.Slot = reader.ReadUInt32();
            auto& c = reward.Character;
            c.CharacterId = reader.ReadUInt64(); c.AccountId = reader.ReadUInt64(); c.CharacterName = reader.ReadUtf16();
            c.CharacterType = reader.ReadUInt32(); c.ColorIndex = reader.ReadUInt32();
            reward.Before = ReadStatistics(reader); c.Statistics = ReadStatistics(reader);
            result.Rewards.push_back(std::move(reward));
        }
        reader.RequireEnd();
        return result;
    }

    Protocol::GameScoreNotification RoomMatchResult::Scores(const RoomMatchRequest& request) const
    {
        Validate(request);
        Protocol::GameScoreNotification result;
        result.ElapsedMilliseconds = request.DurationMilliseconds;
        auto& scores = result.Scores;
        scores.ByteAt452 = request.Winner(); // 42A340: 0 draw, 1/2 explicit team.
        scores.Triples.resize(36); // Native constructor capacity; third column native zero table.
        scores.Triples[0][0] = request.TeamScores[0]; scores.Triples[1][0] = request.TeamScores[1];
        for (std::size_t i = 0; i < Rewards.size(); ++i)
        {
            const auto& reward = Rewards[i]; const auto& member = request.Members[i];
            const auto& c = reward.Character; const auto& s = c.Statistics;
            Protocol::GameScorePlayerRecord p;
            p.ValueAt500 = 1; p.ValueAt0 = static_cast<std::uint32_t>(c.CharacterId);
            p.TextAt4 = c.CharacterName; p.ByteAt52 = member.Team;
            const auto put = [&](std::size_t offset, std::uint32_t value)
            { Put(p.BeforeTextOffsets, p.ValuesBeforeTexts, offset, value); };
            put(56, s.Level); put(60, member.Scores[0]); put(64, member.Scores[1]); put(80, member.Scores[2]);
            const auto award = request.Rules.Awards.Evaluate(member.Scores);
            put(84, request.FormatVersion >= 2 ? award.Contribution : member.Scores[1]);
            const auto range = request.Rules.ExperienceRange(s.Experience);
            put(368, range[0]); put(372, s.Experience); put(376, range[1]);
            put(380, s.Experience - reward.Before.Experience); put(384, s.Lucci - reward.Before.Lucci);
            // +388 remains native reset false: its special animation has not
            // been established. S105 carries the committed level explicitly.
            Put(p.AfterTextOffsets, p.ValuesAfterTexts, 236, member.Scores[3]);
            // C48D20 reads +392 as Rank_list frame; it is not the medal ID.
            Put(p.AfterTextOffsets, p.ValuesAfterTexts, 392, request.FormatVersion >= 2 ? award.Grade : 0xFFFFFFFFU);
            p.ValueInSecondTail = 0xFFFFFFFFU;
            if (request.FormatVersion >= 2 && award.Medal != 0)
            {
                // C70280: relative medal fallback and explicit BF_title mode*10000+ID.
                p.ValueInFirstTail = award.Medal;
                p.ValueInSecondTail = 10000U + award.Medal;
            }
            scores.Players.emplace(static_cast<std::uint8_t>(member.Slot), std::move(p));
        }
        return result;
    }

    std::vector<std::pair<std::uint32_t, Protocol::TcpPacket>> RoomMatchResult::Profiles(const RoomMatchRequest& request) const
    {
        Validate(request);
        std::vector<std::pair<std::uint32_t, Protocol::TcpPacket>> result;
        for (const auto& reward : Rewards)
        {
            Protocol::UserStatNotification profile;
            // Same recovered FighterInfo baseline as LocalMenuData.
            profile.Fighter.ValuesAt296 = {1000, 1};
            for (std::uint16_t key = 0; key != 6; ++key) { profile.Fighter.Abilities.push_back({key, 0}); }
            profile = Characters::CharacterProfile::BindKnownFields(std::move(profile), reward.Character, reward.Character.AccountId);
            const auto range = request.Rules.ExperienceRange(reward.Character.Statistics.Experience);
            profile.Values[2] = range[0]; profile.Values[3] = range[1];
            result.emplace_back(reward.Slot, profile.Encode());
        }
        return result;
    }
}
