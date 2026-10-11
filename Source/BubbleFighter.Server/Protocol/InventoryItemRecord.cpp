#include "InventoryItemRecord.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        void CountEntries(std::size_t count, std::size_t& total)
        {
            if (count > InventoryItemRecord::MaxCollectionEntries ||
                count > InventoryItemRecord::MaxTotalCollectionEntries - total)
            { throw std::invalid_argument("Inventory collection exceeds local limits."); }
            total += count;
        }

        void CountBytes(std::size_t count, std::size_t& total)
        {
            if (count > InventoryItemRecord::MaxUncompressedBytes - total)
            { throw std::invalid_argument("Inventory data exceeds local byte limit."); }
            total += count;
        }

        void CountText(std::u16string_view value, std::size_t& total)
        {
            if (value.size() > PacketWriter::MaxStringCodeUnits)
            { throw std::invalid_argument("Inventory text exceeds local string limit."); }
            CountBytes(2 * value.size(), total);
        }

        std::uint32_t ReadCount(PacketReader& reader, std::size_t minimumBytes, std::size_t& total)
        {
            const auto count = reader.ReadUInt32();
            CountEntries(count, total);
            if (count > reader.RemainingBytes() / minimumBytes)
            { throw std::invalid_argument("Truncated inventory collection."); }
            return count;
        }
    }

    std::vector<std::uint8_t> InventoryItemRecord::EncodeList(std::span<const InventoryItemRecord> records)
    {
        // Preflight the entire body before allocating a potentially large byte
        // vector. Fixed sizes include each string/vector count on the wire.
        std::size_t entries = 0;
        std::size_t bytes = 4;
        CountEntries(records.size(), entries);
        for (const auto& record : records)
        {
            CountBytes(25, bytes);
            CountText(record.TextAt36, bytes);
            CountEntries(record.AttributesAt24.size(), entries);
            for (const auto& attribute : record.AttributesAt24)
            {
                CountBytes(20, bytes);
                CountText(attribute.TextAt24, bytes);
                CountEntries(attribute.ValuesAt8.size(), entries);
                CountBytes(4 * attribute.ValuesAt8.size(), bytes);
            }
        }

        PacketWriter writer;
        writer.WriteUInt32(static_cast<std::uint32_t>(records.size()));
        for (const auto& record : records)
        {
            writer.WriteUInt32(record.Key);
            writer.WriteUInt32(record.ValueAt4);
            writer.WriteUInt64(record.BitsAt8);
            writer.WriteUtf16(record.TextAt36);
            writer.WriteUInt32(static_cast<std::uint32_t>(record.AttributesAt24.size()));
            for (const auto& attribute : record.AttributesAt24)
            {
                writer.WriteUInt32(attribute.ValueAt0);
                writer.WriteUInt32(attribute.ValueAt4);
                writer.WriteUInt32(static_cast<std::uint32_t>(attribute.ValuesAt8.size()));
                for (const auto value : attribute.ValuesAt8) { writer.WriteUInt32(value); }
                writer.WriteUInt32(attribute.ValueAt20);
                writer.WriteUtf16(attribute.TextAt24);
            }
            writer.WriteUInt8(record.Flags);
        }
        return writer.TakeBytes();
    }

    std::vector<InventoryItemRecord> InventoryItemRecord::DecodeList(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() > MaxUncompressedBytes)
        { throw std::invalid_argument("Inventory data exceeds local byte limit."); }
        PacketReader reader(bytes);
        std::size_t entries = 0;
        const auto count = ReadCount(reader, 25, entries);
        std::vector<InventoryItemRecord> records;
        records.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            InventoryItemRecord record;
            record.Key = reader.ReadUInt32();
            record.ValueAt4 = reader.ReadUInt32();
            record.BitsAt8 = reader.ReadUInt64();
            record.TextAt36 = reader.ReadUtf16();
            const auto attributeCount = ReadCount(reader, 20, entries);
            record.AttributesAt24.reserve(attributeCount);
            for (std::uint32_t attributeIndex = 0; attributeIndex < attributeCount; ++attributeIndex)
            {
                InventoryAttributeRecord attribute;
                attribute.ValueAt0 = reader.ReadUInt32();
                attribute.ValueAt4 = reader.ReadUInt32();
                const auto valueCount = ReadCount(reader, 4, entries);
                attribute.ValuesAt8.reserve(valueCount);
                for (std::uint32_t valueIndex = 0; valueIndex < valueCount; ++valueIndex)
                { attribute.ValuesAt8.push_back(reader.ReadUInt32()); }
                attribute.ValueAt20 = reader.ReadUInt32();
                attribute.TextAt24 = reader.ReadUtf16();
                record.AttributesAt24.push_back(std::move(attribute));
            }
            record.Flags = reader.ReadUInt8();
            records.push_back(std::move(record));
        }
        reader.RequireEnd();
        return records;
    }
}
