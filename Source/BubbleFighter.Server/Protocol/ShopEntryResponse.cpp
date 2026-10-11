#include "ShopEntryResponse.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Protocol
{
    namespace
    {
        constexpr std::size_t PriceBytes = 26;
        constexpr std::size_t ItemMinimumBytes = 74;
        constexpr std::size_t CategoryMinimumBytes = 16;

        void CountEntries(std::size_t count, std::size_t& total)
        {
            if (count > ShopEntryResponse::MaxCollectionEntries ||
                count > ShopEntryResponse::MaxTotalCollectionEntries - total)
            { throw std::invalid_argument("Shop collection exceeds local limits."); }
            total += count;
        }

        void CountBytes(std::size_t count, std::size_t& total)
        {
            if (count > ShopEntryResponse::MaxPayloadBytes - total)
            { throw std::invalid_argument("Shop response exceeds local byte limit."); }
            total += count;
        }

        void CountText(std::u16string_view text, std::size_t& total)
        {
            if (text.size() > PacketWriter::MaxStringCodeUnits)
            { throw std::invalid_argument("Shop text exceeds local string limit."); }
            CountBytes(2 * text.size(), total);
        }

        std::uint32_t ReadCount(PacketReader& reader, std::size_t minimumBytes, std::size_t& entries)
        {
            const auto count = reader.ReadUInt32();
            CountEntries(count, entries);
            if (count > reader.RemainingBytes() / minimumBytes)
            { throw std::invalid_argument("Truncated shop collection."); }
            return count;
        }

        void WritePrice(PacketWriter& writer, const ShopPriceRecord& price)
        {
            writer.WriteUInt32(price.StockId);
            writer.WriteUInt32(price.ItemId);
            writer.WriteUInt32(price.FlagAt12 ? 1U : 0U);
            writer.WriteUInt16(price.Type);
            writer.WriteUInt32(price.Duration);
            writer.WriteUInt32(price.ShowPrice);
            writer.WriteUInt32(price.SellPrice);
        }

        ShopPriceRecord ReadPrice(PacketReader& reader)
        {
            ShopPriceRecord price;
            price.StockId = reader.ReadUInt32();
            price.ItemId = reader.ReadUInt32();
            price.FlagAt12 = reader.ReadUInt32() != 0;
            price.Type = reader.ReadUInt16();
            price.Duration = reader.ReadUInt32();
            price.ShowPrice = reader.ReadUInt32();
            price.SellPrice = reader.ReadUInt32();
            return price;
        }
    }

    TcpPacket ShopEntryResponse::Encode() const
    {
        // Preflight all nested counts and UTF-16 strings before allocating the
        // output buffer. These are local bounds; opaque values are preserved.
        std::size_t entries = 0;
        std::size_t bytes = 40;
        CountEntries(Items.size(), entries);
        for (const auto& item : Items)
        {
            CountBytes(ItemMinimumBytes, bytes);
            CountText(item.Name, bytes);
            CountEntries(item.Prices.size(), entries);
            CountBytes(PriceBytes * item.Prices.size(), bytes);
        }
        CountEntries(Categories.size(), entries);
        for (const auto& category : Categories)
        {
            CountBytes(CategoryMinimumBytes, bytes);
            CountText(category.Name, bytes);
            CountEntries(category.SubCategories.size(), entries);
            for (const auto& subCategory : category.SubCategories)
            {
                CountBytes(CategoryMinimumBytes, bytes);
                CountText(subCategory.Name, bytes);
            }
        }
        CountEntries(ValuesAt56.size(), entries);
        CountBytes(4 * ValuesAt56.size(), bytes);

        PacketWriter writer;
        writer.WriteUInt32(ValueAt0);
        writer.WriteUInt32(RefreshCatalog ? 1U : 0U);
        writer.WriteUInt64(CatalogRevision);
        writer.WriteUInt32(static_cast<std::uint32_t>(Items.size()));
        for (const auto& item : Items)
        {
            writer.WriteUInt32(item.Identifier);
            writer.WriteUInt32(item.Kind);
            writer.WriteUtf16(item.Name);
            writer.WriteUInt32(item.ValueAt36);
            writer.WriteUInt32(item.MainCategory);
            writer.WriteUInt32(item.SubCategory);
            writer.WriteUInt32(item.FlagAt48 ? 1U : 0U);
            writer.WriteUInt64(item.WideAt56);
            writer.WriteUInt64(item.WideAt64);
            WritePrice(writer, item.Price);
            writer.WriteUInt32(static_cast<std::uint32_t>(item.Prices.size()));
            for (const auto& price : item.Prices) { WritePrice(writer, price); }
        }
        writer.WriteUInt64(WideAt32);
        writer.WriteUInt32(static_cast<std::uint32_t>(Categories.size()));
        for (const auto& category : Categories)
        {
            writer.WriteUInt32(category.Identifier);
            writer.WriteUtf16(category.Name);
            writer.WriteUInt32(category.DisplayOrder);
            writer.WriteUInt32(static_cast<std::uint32_t>(category.SubCategories.size()));
            for (const auto& subCategory : category.SubCategories)
            {
                writer.WriteUInt32(subCategory.Identifier);
                writer.WriteUtf16(subCategory.Name);
                writer.WriteUInt32(subCategory.ValueAt32);
                writer.WriteUInt32(subCategory.ValueAt36);
            }
        }
        writer.WriteUInt32(static_cast<std::uint32_t>(ValuesAt56.size()));
        for (const auto value : ValuesAt56) { writer.WriteUInt32(value); }
        writer.WriteUInt32(SelectedCategory);
        return {MessageId, writer.TakeBytes()};
    }

    ShopEntryResponse ShopEntryResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Incorrect shop entry response message."); }
        if (packet.Payload.size() > MaxPayloadBytes)
        { throw std::invalid_argument("Shop response exceeds local byte limit."); }
        PacketReader reader(packet.Payload);
        std::size_t entries = 0;
        ShopEntryResponse response;
        response.ValueAt0 = reader.ReadUInt32();
        response.RefreshCatalog = reader.ReadUInt32() != 0;
        response.CatalogRevision = reader.ReadUInt64();
        const auto itemCount = ReadCount(reader, ItemMinimumBytes, entries);
        response.Items.reserve(itemCount);
        for (std::uint32_t index = 0; index < itemCount; ++index)
        {
            ShopItemCardRecord item;
            item.Identifier = reader.ReadUInt32();
            item.Kind = reader.ReadUInt32();
            item.Name = reader.ReadUtf16();
            item.ValueAt36 = reader.ReadUInt32();
            item.MainCategory = reader.ReadUInt32();
            item.SubCategory = reader.ReadUInt32();
            item.FlagAt48 = reader.ReadUInt32() != 0;
            item.WideAt56 = reader.ReadUInt64();
            item.WideAt64 = reader.ReadUInt64();
            item.Price = ReadPrice(reader);
            const auto priceCount = ReadCount(reader, PriceBytes, entries);
            item.Prices.reserve(priceCount);
            for (std::uint32_t priceIndex = 0; priceIndex < priceCount; ++priceIndex)
            { item.Prices.push_back(ReadPrice(reader)); }
            response.Items.push_back(std::move(item));
        }
        response.WideAt32 = reader.ReadUInt64();
        const auto categoryCount = ReadCount(reader, CategoryMinimumBytes, entries);
        response.Categories.reserve(categoryCount);
        for (std::uint32_t index = 0; index < categoryCount; ++index)
        {
            ShopMainCategoryRecord category;
            category.Identifier = reader.ReadUInt32();
            category.Name = reader.ReadUtf16();
            category.DisplayOrder = reader.ReadUInt32();
            const auto subCount = ReadCount(reader, CategoryMinimumBytes, entries);
            category.SubCategories.reserve(subCount);
            for (std::uint32_t subIndex = 0; subIndex < subCount; ++subIndex)
            {
                ShopSubCategoryRecord subCategory;
                subCategory.Identifier = reader.ReadUInt32();
                subCategory.Name = reader.ReadUtf16();
                subCategory.ValueAt32 = reader.ReadUInt32();
                subCategory.ValueAt36 = reader.ReadUInt32();
                category.SubCategories.push_back(std::move(subCategory));
            }
            response.Categories.push_back(std::move(category));
        }
        const auto valueCount = ReadCount(reader, 4, entries);
        response.ValuesAt56.reserve(valueCount);
        for (std::uint32_t index = 0; index < valueCount; ++index)
        { response.ValuesAt56.push_back(reader.ReadUInt32()); }
        response.SelectedCategory = reader.ReadUInt32();
        reader.RequireEnd();
        return response;
    }
}
