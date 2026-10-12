#include "ClientToServerMessage0x0243.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>
#include <type_traits>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        constexpr std::size_t MaxBytes = TcpPacketCodec::DefaultMaxPayloadSize;
        void AddSize(std::size_t& size, std::size_t count, std::size_t width)
        {
            if (count > (MaxBytes - size) / width) { throw std::length_error("C579 exceeds payload policy."); }
            size += count * width;
        }
        template<typename T> void WriteList(PacketWriter& writer, const std::vector<T>& values)
        {
            writer.WriteUInt32(static_cast<std::uint32_t>(values.size()));
            for (const auto& value : values)
            {
                if constexpr (std::is_same_v<T, std::uint32_t>) { writer.WriteUInt32(value); }
                else if constexpr (std::is_same_v<T, std::uint8_t>) { writer.WriteUInt8(value); }
                else if constexpr (std::is_same_v<T, std::u16string>) { writer.WriteUtf16(value); }
                else { for (const auto word : value) { writer.WriteUInt32(word); } }
            }
        }
        template<typename T> void ReadList(PacketReader& reader, std::vector<T>& values, std::size_t width)
        {
            const auto count = reader.ReadUInt32();
            // Check against remaining bytes before reserve, including strings'
            // minimum four-byte length field. No client count drives allocation unchecked.
            if (count > reader.RemainingBytes() / width) { throw std::runtime_error("Truncated C579 list."); }
            values.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                if constexpr (std::is_same_v<T, std::uint32_t>) { values.push_back(reader.ReadUInt32()); }
                else if constexpr (std::is_same_v<T, std::uint8_t>) { values.push_back(reader.ReadUInt8()); }
                else if constexpr (std::is_same_v<T, std::u16string>) { values.push_back(reader.ReadUtf16()); }
                else { values.push_back({reader.ReadUInt32(), reader.ReadUInt32(), reader.ReadUInt32()}); }
            }
        }
    }

    TcpPacket ClientToServerMessage0x0243::Encode() const
    {
        std::size_t size = 60; // One scalar and fourteen independent list counts.
        for (const auto* values : {&TriplesAt4, &TriplesAt76, &TriplesAt124}) { AddSize(size, values->size(), 12); }
        for (const auto* values : {&ValuesAt16, &ValuesAt40, &ValuesAt52, &ValuesAt64,
            &ValuesAt88, &ValuesAt100, &ValuesAt112, &ValuesAt136, &ValuesAt148})
        { AddSize(size, values->size(), 4); }
        AddSize(size, BytesAt28.size(), 1);
        AddSize(size, StringsAt160.size(), 4);
        for (const auto& value : StringsAt160)
        {
            if (value.size() > PacketWriter::MaxStringCodeUnits) { throw std::length_error("C579 string exceeds policy."); }
            AddSize(size, value.size(), 2);
        }
        PacketWriter writer;
        writer.WriteUInt32(ValueAt0);
        WriteList(writer, TriplesAt4); WriteList(writer, ValuesAt16); WriteList(writer, BytesAt28);
        WriteList(writer, ValuesAt40); WriteList(writer, ValuesAt52); WriteList(writer, ValuesAt64);
        WriteList(writer, TriplesAt76); WriteList(writer, ValuesAt88); WriteList(writer, ValuesAt100);
        WriteList(writer, ValuesAt112); WriteList(writer, TriplesAt124); WriteList(writer, ValuesAt136);
        WriteList(writer, ValuesAt148); WriteList(writer, StringsAt160);
        return {MessageId, writer.TakeBytes()};
    }

    ClientToServerMessage0x0243 ClientToServerMessage0x0243::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid C579 message id."); }
        if (packet.Payload.size() > MaxBytes) { throw std::length_error("C579 exceeds payload policy."); }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x0243 result;
        result.ValueAt0 = reader.ReadUInt32();
        ReadList(reader, result.TriplesAt4, 12); ReadList(reader, result.ValuesAt16, 4);
        ReadList(reader, result.BytesAt28, 1); ReadList(reader, result.ValuesAt40, 4);
        ReadList(reader, result.ValuesAt52, 4); ReadList(reader, result.ValuesAt64, 4);
        ReadList(reader, result.TriplesAt76, 12); ReadList(reader, result.ValuesAt88, 4);
        ReadList(reader, result.ValuesAt100, 4); ReadList(reader, result.ValuesAt112, 4);
        ReadList(reader, result.TriplesAt124, 12); ReadList(reader, result.ValuesAt136, 4);
        ReadList(reader, result.ValuesAt148, 4); ReadList(reader, result.StringsAt160, 4);
        reader.RequireEnd();
        return result;
    }
}
