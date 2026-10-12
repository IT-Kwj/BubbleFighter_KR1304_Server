#pragma once
#include "PacketReader.h"
#include "PacketWriter.h"
#include <array>
#include <map>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // E073: native ScoreData player objects have stride 512. Offsets below
    // describe the client object, not padding or a native server structure.
    struct GameScorePlayerRecord
    {
        std::uint32_t ValueAt500 = 0;
        std::uint32_t ValueAt0 = 0;
        std::u16string TextAt4;
        std::uint8_t ByteAt52 = 0;
        static constexpr std::array<std::size_t, 29> BeforeTextOffsets{
            56,60,64,68,80,72,84,88,92,96,104,100,108,112,120,124,128,176,
            132,136,140,364,368,372,376,380,384,388,436};
        // Offset 388 is a native bool, zero-extended to uint32 on the wire.
        // Keep the raw wire value; reading it does not authorize any reward.
        std::array<std::uint32_t, 29> ValuesBeforeTexts{};
        std::vector<std::u16string> TextsAt464;
        std::u16string TextAt440;
        static constexpr std::array<std::size_t, 7> AfterTextOffsets{236,476,480,392,144,148,408};
        std::array<std::uint32_t, 7> ValuesAfterTexts{};
        std::uint16_t ShortAt412 = 0;
        static constexpr std::array<std::size_t, 5> AfterShortOffsets{416,420,160,164,168};
        std::array<std::uint32_t, 5> ValuesAfterShort{};
        std::array<std::uint8_t, 2> BytesAt172{};
        std::uint16_t ShortAt184 = 0;
        std::uint16_t ShortAt76 = 0;
        std::uint16_t ShortAt186 = 0;
        std::uint32_t ValueAt508 = 0;
        std::uint16_t ShortAt188 = 0;
        std::uint8_t ByteAt432 = 0;
        static constexpr std::array<std::size_t, 10> TailOffsets{192,196,200,204,208,212,216,220,424,428};
        std::array<std::uint32_t, 10> TailValues{};
        // Two separate passes after all player records, indexed by the same mask.
        std::uint32_t ValueInFirstTail = 0; // ScoreData+18892+slot*4
        std::uint32_t ValueInSecondTail = 0; // ScoreData+19036+slot*4
        bool operator==(const GameScorePlayerRecord&) const = default;
    };

    struct GameScoreRecord
    {
        static constexpr std::size_t SlotCount = 36;
        static constexpr std::size_t MaxPayloadBytes = 65536; // Local allocation policy.
        static constexpr std::size_t MaxTotalTexts = 1024; // Across all player string lists.
        // Native count is a signed byte, followed by this raw byte and uint32.
        // Values are not named winner/team/reward without consumer evidence.
        std::uint8_t ByteAt452 = 0;
        std::uint32_t ValueAt456 = 0;
        std::vector<std::array<std::uint32_t, 3>> Triples;
        std::map<std::uint8_t, GameScorePlayerRecord> Players;

        std::size_t EncodedSize() const;
        void Write(PacketWriter& writer) const;
        static GameScoreRecord Read(PacketReader& reader);
        bool operator==(const GameScoreRecord&) const = default;
    };
}
