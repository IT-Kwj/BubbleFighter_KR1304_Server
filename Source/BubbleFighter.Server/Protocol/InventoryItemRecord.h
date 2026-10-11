#pragma once

#include "ZlibStoredStream.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    struct InventoryAttributeRecord
    {
        // E052: 0x010D9410. Offsets are in the client object, not the wire.
        std::uint32_t ValueAt0 = 0;
        std::uint32_t ValueAt4 = 0;
        std::vector<std::uint32_t> ValuesAt8;
        std::uint32_t ValueAt20 = 0;
        std::u16string TextAt24;
        bool operator==(const InventoryAttributeRecord&) const = default;
    };

    struct InventoryItemRecord
    {
        // E052: S802 decompresses before 0x010D8550 reads this collection.
        // E053 identifies Key as item ID, ValueAt4 as count/Life, BitsAt8 as
        // remaining days (IEEE double), and bits 0/1 as equipped/exhausted.
        // Keep raw names here for lossless wire decoding; the Inventory feature
        // validates ordinary slots and computes duration from stored expiry.
        static constexpr std::uint32_t MaxCollectionEntries = 1024;
        static constexpr std::uint32_t MaxTotalCollectionEntries = 4096;
        static constexpr std::size_t MaxUncompressedBytes = ZlibStoredStream::MaxInputBytes;

        std::uint32_t Key = 0;
        std::uint32_t ValueAt4 = 0;
        std::uint64_t BitsAt8 = 0;
        std::u16string TextAt36;
        std::vector<InventoryAttributeRecord> AttributesAt24;
        // Wire bits map to client bytes +16,+17,+18,+19,+20,+22,+21,+23.
        std::uint8_t Flags = 0;

        static std::vector<std::uint8_t> EncodeList(std::span<const InventoryItemRecord> records);
        static std::vector<InventoryItemRecord> DecodeList(std::span<const std::uint8_t> bytes);
        bool operator==(const InventoryItemRecord&) const = default;
    };
}
