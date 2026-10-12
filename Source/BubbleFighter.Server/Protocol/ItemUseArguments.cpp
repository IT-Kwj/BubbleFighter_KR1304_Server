#include "ItemUseArguments.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    void ItemUseArguments::Write(PacketWriter& writer, const std::vector<std::u16string>& values)
    {
        if (values.size() > MaxCount) { throw std::invalid_argument("Too many item use arguments."); }
        std::size_t bytes = 12; // two fields and vector count
        for (const auto& value : values)
        {
            if (value.size() > PacketWriter::MaxStringCodeUnits ||
                4 + value.size() * 2 > MaxPayloadBytes - bytes)
            { throw std::invalid_argument("Item use arguments exceed local limits."); }
            bytes += 4 + value.size() * 2;
        }
        writer.WriteUInt32(static_cast<std::uint32_t>(values.size()));
        for (const auto& value : values) { writer.WriteUtf16(value); }
    }
    std::vector<std::u16string> ItemUseArguments::Read(PacketReader& reader)
    {
        const auto count = reader.ReadUInt32();
        if (count > MaxCount || count > reader.RemainingBytes() / 4)
        { throw std::invalid_argument("Invalid item use argument count."); }
        std::vector<std::u16string> values;
        values.reserve(count);
        for (std::uint32_t i = 0; i != count; ++i) { values.push_back(reader.ReadUtf16()); }
        return values;
    }
}
