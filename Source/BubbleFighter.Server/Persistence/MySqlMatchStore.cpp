#include "MySqlMatchStore.h"
#include "MySqlCommands.h"
#include <algorithm>
#include <array>

namespace BubbleFighter::Server::Persistence
{
    using namespace MySqlCommands;
    using namespace Features::Rooms;

    MySqlMatchStore::MySqlMatchStore(const std::filesystem::path& configuration) : connection_(configuration)
    {
        Statement schema(connection_.Handle(), "SELECT MatchId, RequestData, ResultData FROM MatchSettlements WHERE 1 = 0");
    }

    void MySqlMatchStore::SavePending(const RoomMatchRequest& request)
    {
        const auto input = request.Encode();
        const auto key = Bytes(request.MatchId.data(), static_cast<unsigned long>(request.MatchId.size()));
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        {
            Statement insert(connection, "INSERT INTO MatchSettlements (MatchId, RequestData, ResultData) VALUES (?, ?, '') ON DUPLICATE KEY UPDATE MatchId = MatchId");
            std::array parameters{key, Bytes(input.data(), static_cast<unsigned long>(input.size()))}; insert.Execute(parameters);
        }
        bool completed = false;
        {
            Statement query(connection, "SELECT RequestData, OCTET_LENGTH(ResultData) FROM MatchSettlements WHERE MatchId = ? FOR UPDATE");
            std::array parameters{key}; query.Execute(parameters);
            std::array<std::uint8_t, 4096> saved{}; unsigned long length = 0; std::uint64_t resultLength = 0;
            std::array values{Bytes(saved.data(), static_cast<unsigned long>(saved.size())), Number(resultLength)};
            values[0].length = &length;
            if (!query.Fetch(values) || length != input.size() || !std::equal(input.begin(), input.end(), saved.begin()))
            { throw std::runtime_error("Changed durable match request."); }
            completed = resultLength != 0;
        }
        if (!completed)
        {
            // Reject mismatched identities before making a new request durable.
            // Use the same ascending character lock order as reward updates.
            auto members = request.Members;
            std::sort(members.begin(), members.end(), [](const auto& a, const auto& b) { return a.CharacterId < b.CharacterId; });
            for (const auto& member : members)
            {
                Statement query(connection, "SELECT c.CharacterId FROM Characters c JOIN Accounts a ON a.AccountId = c.AccountId WHERE c.CharacterId = ? AND c.AccountId = ? AND a.IsDisabled = 0 FOR UPDATE");
                std::array parameters{Number(member.CharacterId), Number(member.AccountId)}; query.Execute(parameters);
                std::uint64_t character = 0; std::array values{Number(character)};
                if (!query.Fetch(values) || character != member.CharacterId) { throw std::runtime_error("Frozen match owner is unavailable."); }
            }
        }
        transaction.Commit();
    }

    std::vector<RoomMatchRequest> MySqlMatchStore::LoadPending(const std::array<std::uint8_t, 16>& after)
    {
        // Keyset pagination ensures one unavailable old account cannot starve
        // all later requests. A worker resets its cursor after an empty batch.
        Statement query(connection_.Handle(), "SELECT MatchId, RequestData FROM MatchSettlements WHERE OCTET_LENGTH(ResultData) = 0 AND MatchId > ? ORDER BY MatchId LIMIT 64");
        std::array parameters{Bytes(after.data(), static_cast<unsigned long>(after.size()))}; query.Execute(parameters);
        std::vector<RoomMatchRequest> result;
        std::array<std::uint8_t, 16> id{}; std::array<std::uint8_t, 4096> input{};
        unsigned long idLength = 0, length = 0;
        std::array values{Bytes(id.data(), static_cast<unsigned long>(id.size())), Bytes(input.data(), static_cast<unsigned long>(input.size()))};
        values[0].length = &idLength; values[1].length = &length;
        while (query.Fetch(values))
        {
            if (idLength != id.size() || length > input.size()) { throw std::runtime_error("Invalid pending match row."); }
            auto request = RoomMatchRequest::Decode(std::span(input).first(length));
            if (request.MatchId != id) { throw std::runtime_error("Pending match identity mismatch."); }
            result.push_back(std::move(request));
        }
        return result;
    }

    RoomMatchResult MySqlMatchStore::Settle(const RoomMatchRequest& request)
    {
        SavePending(request);
        const auto input = request.Encode();
        const auto key = Bytes(request.MatchId.data(), static_cast<unsigned long>(request.MatchId.size()));
        auto* connection = connection_.Handle();
        Transaction transaction(connection);
        {
            // Unique match lock first. Duplicate calls wait and read the exact
            // receipt; changed inputs are rejected, including changed rules.
            Statement insert(connection, "INSERT INTO MatchSettlements (MatchId, RequestData, ResultData) VALUES (?, ?, '') ON DUPLICATE KEY UPDATE MatchId = MatchId");
            std::array parameters{key, Bytes(input.data(), static_cast<unsigned long>(input.size()))};
            insert.Execute(parameters);
        }
        std::vector<std::uint8_t> savedResult;
        {
            Statement query(connection, "SELECT RequestData, ResultData FROM MatchSettlements WHERE MatchId = ? FOR UPDATE");
            std::array parameters{key}; query.Execute(parameters);
            std::array<std::uint8_t, 4096> savedInput{}, receipt{};
            unsigned long inputLength = 0, resultLength = 0;
            std::array values{Bytes(savedInput.data(), static_cast<unsigned long>(savedInput.size())),
                Bytes(receipt.data(), static_cast<unsigned long>(receipt.size()))};
            values[0].length = &inputLength; values[1].length = &resultLength;
            if (!query.Fetch(values) || inputLength != input.size() || !std::equal(input.begin(), input.end(), savedInput.begin()))
            { throw std::runtime_error("Match identity reused with different settlement inputs."); }
            if (resultLength > receipt.size()) { throw std::runtime_error("Invalid stored match receipt."); }
            savedResult.assign(receipt.begin(), receipt.begin() + resultLength);
        }
        if (!savedResult.empty())
        {
            auto result = RoomMatchResult::Decode(savedResult);
            result.Validate(request);
            transaction.Commit();
            return result;
        }

        RoomMatchResult result;
        result.Rewards.resize(request.Members.size());
        std::vector<std::size_t> order;
        for (std::size_t i = 0; i < request.Members.size(); ++i) { order.push_back(i); }
        std::sort(order.begin(), order.end(), [&](auto a, auto b)
            { return request.Members[a].CharacterId < request.Members[b].CharacterId; });
        for (auto index : order)
        {
            const auto& member = request.Members[index];
            auto& reward = result.Rewards[index];
            reward.Slot = member.Slot;
            auto& c = reward.Character;
            c.AccountId = member.AccountId; c.CharacterId = member.CharacterId;
            {
                // Same owner/character lock order as inventory writers. Values
                // are read under lock; no cached balance can overwrite a purchase.
                Statement query(connection, "SELECT c.NameUtf16, c.CharacterType, c.ColorIndex, c.Level, c.Experience, c.Lucci, c.Cash FROM Characters c JOIN Accounts a ON a.AccountId = c.AccountId WHERE c.CharacterId = ? AND c.AccountId = ? AND a.IsDisabled = 0 FOR UPDATE");
                std::array parameters{Number(c.CharacterId), Number(c.AccountId)}; query.Execute(parameters);
                std::array<std::uint8_t, 32> name{}; unsigned long length = 0;
                std::array values{Bytes(name.data(), static_cast<unsigned long>(name.size())), Number(c.CharacterType), Number(c.ColorIndex),
                    Number(c.Statistics.Level), Number(c.Statistics.Experience), Number(c.Statistics.Lucci), Number(c.Statistics.Cash)};
                values[0].length = &length;
                if (!query.Fetch(values) || length == 0 || length > name.size() || length % 2 != 0)
                { throw std::runtime_error("Match participant is unavailable."); }
                Protocol::PacketReader reader(std::span(name).first(length));
                while (reader.RemainingBytes()) { c.CharacterName.push_back(reader.ReadUInt16()); }
            }
            reward.Before = c.Statistics;
            c.Statistics = request.Rules.Apply(reward.Before, member.Scores, member.Team == request.Winner());
        }
        // Fully validate and encode every response before writing any reward.
        result.Scores(request).Encode(); result.Profiles(request);
        const auto receipt = result.Encode();
        for (auto index : order)
        {
            const auto& reward = result.Rewards[index]; const auto& c = reward.Character;
            if (c.Statistics == reward.Before) { continue; }
            Statement update(connection, "UPDATE Characters SET Level = ?, Experience = ?, Lucci = ? WHERE CharacterId = ? AND AccountId = ?");
            std::array parameters{Number(c.Statistics.Level), Number(c.Statistics.Experience), Number(c.Statistics.Lucci),
                Number(c.CharacterId), Number(c.AccountId)}; update.Execute(parameters);
            if (update.AffectedRows() != 1) { throw std::runtime_error("Match reward update failed."); }
        }
        {
            Statement update(connection, "UPDATE MatchSettlements SET ResultData = ?, CommittedAt = CURRENT_TIMESTAMP(6) WHERE MatchId = ?");
            std::array parameters{Bytes(receipt.data(), static_cast<unsigned long>(receipt.size())), key}; update.Execute(parameters);
            if (update.AffectedRows() != 1) { throw std::runtime_error("Match receipt update failed."); }
        }
        transaction.Commit();
        return result;
    }
}
