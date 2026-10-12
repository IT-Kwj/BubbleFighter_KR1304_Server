#include "FighterAttributeCatalog.h"
#include "FighterAttributeCatalogData.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Battle
{
    std::optional<FighterBaseAttributes> FighterAttributeCatalog::Resolve(std::uint32_t character,
        std::uint32_t channelTableKey, std::uint32_t modeTableKey, FighterAttributeVariant variant)
    {
        if (variant != FighterAttributeVariant::Normal && variant != FighterAttributeVariant::Season &&
            variant != FighterAttributeVariant::BubbleCup)
        { throw std::invalid_argument("Unknown fighter attribute variant."); }
        const std::array tables{0U, 1U, 2U, 3U + static_cast<unsigned>(variant)};
        const std::array keys{0U, channelTableKey, modeTableKey, character};
        FighterBaseAttributes result{};
        for (std::size_t i = 0; i < tables.size(); ++i)
        {
            const auto found = std::find_if(std::begin(FighterAttributeRows), std::end(FighterAttributeRows),
                [&](const auto& row) { return row.Table == tables[i] && row.Key == keys[i]; });
            if (found == std::end(FighterAttributeRows)) { return std::nullopt; }
            result.Hp += found->Hp;
            result.BubbleHp = std::max(0.0f, result.BubbleHp + found->BubbleHp);
        }
        return result;
    }
}
