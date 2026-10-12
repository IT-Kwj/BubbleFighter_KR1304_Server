#include "GameScoreRecord.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        template <std::size_t N> void WriteValues(PacketWriter& writer, const std::array<std::uint32_t, N>& values)
        { for (const auto value : values) { writer.WriteUInt32(value); } }
        template <std::size_t N> void ReadValues(PacketReader& reader, std::array<std::uint32_t, N>& values)
        { for (auto& value : values) { value = reader.ReadUInt32(); } }
        void AddTextSize(const std::u16string& text, std::size_t& size)
        {
            if (text.size() > PacketWriter::MaxStringCodeUnits || size > GameScoreRecord::MaxPayloadBytes - 4 ||
                text.size() > (GameScoreRecord::MaxPayloadBytes - size - 4) / 2)
            { throw std::length_error("Game score exceeds local string/payload limit."); }
            size += 4 + text.size() * 2;
        }
    }

    std::size_t GameScoreRecord::EncodedSize() const
    {
        if (Triples.size() > SlotCount || Players.size() > SlotCount)
        { throw std::length_error("Game score exceeds native slot capacity."); }
        std::size_t size = 14 + Triples.size() * 12;
        std::size_t textCount = 0;
        for (const auto& [slot, p] : Players)
        {
            if (slot >= SlotCount) { throw std::invalid_argument("Invalid game score slot."); }
            if (p.TextsAt464.size() > MaxTotalTexts - textCount)
            { throw std::length_error("Game score text collection exceeds local limit."); }
            textCount += p.TextsAt464.size();
            // Scalar fields, list count and both final tail passes; strings counted below.
            constexpr std::size_t fixedBytes = 242;
            if (size > MaxPayloadBytes - fixedBytes) { throw std::length_error("Game score payload too large."); }
            size += fixedBytes;
            AddTextSize(p.TextAt4, size);
            for (const auto& text : p.TextsAt464) { AddTextSize(text, size); }
            AddTextSize(p.TextAt440, size);
        }
        return size;
    }

    void GameScoreRecord::Write(PacketWriter& writer) const
    {
        EncodedSize(); // Validate before appending anything.
        writer.WriteUInt8(static_cast<std::uint8_t>(Triples.size()));
        writer.WriteUInt8(ByteAt452);
        writer.WriteUInt32(ValueAt456);
        for (const auto& triple : Triples) { WriteValues(writer, triple); }
        std::uint64_t mask = 0;
        for (const auto& [slot, player] : Players) { (void)player; mask |= std::uint64_t{1} << slot; }
        writer.WriteUInt64(mask);
        for (const auto& [slot, p] : Players)
        {
            (void)slot;
            writer.WriteUInt32(p.ValueAt500); writer.WriteUInt32(p.ValueAt0);
            writer.WriteUtf16(p.TextAt4); writer.WriteUInt8(p.ByteAt52);
            WriteValues(writer, p.ValuesBeforeTexts);
            writer.WriteUInt32(static_cast<std::uint32_t>(p.TextsAt464.size()));
            for (const auto& text : p.TextsAt464) { writer.WriteUtf16(text); }
            writer.WriteUtf16(p.TextAt440);
            WriteValues(writer, p.ValuesAfterTexts); writer.WriteUInt16(p.ShortAt412);
            WriteValues(writer, p.ValuesAfterShort);
            for (const auto value : p.BytesAt172) { writer.WriteUInt8(value); }
            writer.WriteUInt16(p.ShortAt184); writer.WriteUInt16(p.ShortAt76); writer.WriteUInt16(p.ShortAt186);
            writer.WriteUInt32(p.ValueAt508); writer.WriteUInt16(p.ShortAt188); writer.WriteUInt8(p.ByteAt432);
            WriteValues(writer, p.TailValues);
        }
        for (const auto& [slot, p] : Players) { (void)slot; writer.WriteUInt32(p.ValueInFirstTail); }
        for (const auto& [slot, p] : Players) { (void)slot; writer.WriteUInt32(p.ValueInSecondTail); }
    }

    GameScoreRecord GameScoreRecord::Read(PacketReader& reader)
    {
        if (reader.RemainingBytes() > MaxPayloadBytes) { throw std::length_error("Game score input exceeds local limit."); }
        GameScoreRecord result;
        const auto count = reader.ReadUInt8();
        if (count > SlotCount) { throw std::length_error("Invalid native game score count."); }
        result.ByteAt452 = reader.ReadUInt8(); result.ValueAt456 = reader.ReadUInt32();
        for (std::uint8_t i = 0; i < count; ++i)
        {
            std::array<std::uint32_t, 3> triple{}; ReadValues(reader, triple); result.Triples.push_back(triple);
        }
        const auto mask = reader.ReadUInt64();
        if ((mask >> SlotCount) != 0) { throw std::invalid_argument("Game score mask exceeds native slots."); }
        std::size_t textCount = 0;
        for (std::uint8_t slot = 0; slot < SlotCount; ++slot)
        {
            if ((mask & (std::uint64_t{1} << slot)) == 0) { continue; }
            GameScorePlayerRecord p;
            p.ValueAt500 = reader.ReadUInt32(); p.ValueAt0 = reader.ReadUInt32();
            p.TextAt4 = reader.ReadUtf16(); p.ByteAt52 = reader.ReadUInt8();
            ReadValues(reader, p.ValuesBeforeTexts);
            const auto texts = reader.ReadUInt32();
            if (texts > MaxTotalTexts - textCount || texts > reader.RemainingBytes() / 4)
            { throw std::length_error("Game score text collection invalid or truncated."); }
            textCount += texts;
            for (std::uint32_t i = 0; i < texts; ++i) { p.TextsAt464.push_back(reader.ReadUtf16()); }
            p.TextAt440 = reader.ReadUtf16();
            ReadValues(reader, p.ValuesAfterTexts); p.ShortAt412 = reader.ReadUInt16();
            ReadValues(reader, p.ValuesAfterShort);
            for (auto& value : p.BytesAt172) { value = reader.ReadUInt8(); }
            p.ShortAt184 = reader.ReadUInt16(); p.ShortAt76 = reader.ReadUInt16(); p.ShortAt186 = reader.ReadUInt16();
            p.ValueAt508 = reader.ReadUInt32(); p.ShortAt188 = reader.ReadUInt16(); p.ByteAt432 = reader.ReadUInt8();
            ReadValues(reader, p.TailValues); result.Players.emplace(slot, std::move(p));
        }
        for (auto& [slot, p] : result.Players) { (void)slot; p.ValueInFirstTail = reader.ReadUInt32(); }
        for (auto& [slot, p] : result.Players) { (void)slot; p.ValueInSecondTail = reader.ReadUInt32(); }
        result.EncodedSize();
        return result;
    }
}
