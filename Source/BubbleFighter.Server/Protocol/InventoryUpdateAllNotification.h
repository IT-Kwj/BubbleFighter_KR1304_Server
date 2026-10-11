#pragma once

#include "InventoryItemRecord.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // gspInventoryUpdateAllInventorySlot, E052. Framing is a uint32 compressed
    // length followed by a zlib stream, not a bare inventory record vector.
    struct InventoryUpdateAllNotification
    {
        static constexpr std::uint16_t MessageId = 0x0322;
        // Allows all stored-block output at the existing 256 KiB input limit.
        static constexpr std::size_t MaxCompressedBytes = ZlibStoredStream::MaxInputBytes + 64;

        std::vector<std::uint8_t> CompressedRecords;

        static InventoryUpdateAllNotification FromRecords(std::span<const InventoryItemRecord> records);
        TcpPacket Encode() const;
        // Decodes framing only, preserving arbitrary zlib data. Does not inflate
        // or validate compressed contents. FromRecords creates server output.
        static InventoryUpdateAllNotification Decode(const TcpPacket& packet);
        bool operator==(const InventoryUpdateAllNotification&) const = default;
    };
}
