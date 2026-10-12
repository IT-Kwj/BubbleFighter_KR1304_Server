#include "LocalMenuData.h"
#include "Features/Inventory/EquipmentView.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Authentication
{
    MenuInitializationData LocalMenuData::Create(const Characters::CharacterRecord& character,
        Inventory::CharacterInventory inventory, std::uint64_t observedAtUnixSeconds)
    {
        if (character.AccountId == 0 || character.CharacterId == 0 ||
            character.CharacterId > std::numeric_limits<std::uint32_t>::max() ||
            inventory.AccountId != character.AccountId || inventory.CharacterId != character.CharacterId)
        { throw std::invalid_argument("Local menu inventory ownership is invalid."); }
        MenuInitializationData data;
        data.Equipment = Inventory::EquipmentView::Create(character, inventory, observedAtUnixSeconds);
        constexpr auto missing = std::numeric_limits<std::uint32_t>::max();
        // 0x01159720 / 0x011527E0: native FighterInfo baseline. These are
        // initial local state, not recovered original-server rewards/unlocks.
        data.Profile.Fighter.ValuesAt296 = {1000, 1};
        for (std::uint16_t key = 0; key != 6; ++key)
        { data.Profile.Fighter.Abilities.push_back({key, 0}); }

        auto& descriptor = data.Scene.Descriptor;
        descriptor.SessionType = 1; // Local solo menu, slot zero.
        // 0x0113B7B0 and 0x0113C3E0; resource default from 0x00408000.
        descriptor.ValueAt68 = missing;
        descriptor.WideAt80 = std::numeric_limits<std::uint64_t>::max();
        auto& configuration = descriptor.Configuration;
        configuration.ShortAt4 = 1;
        configuration.TextsAt44[0] = u"Random";
        configuration.ValueAt140 = 1;
        configuration.TextsAt144[0] = u"Random";
        configuration.TextsAt144[2] = u"Random";
        configuration.TextsAt144[4] = u"Random";
        configuration.BooleansAt322[2] = 1;
        configuration.BooleansAt340 = {1, 1};
        configuration.ValueAt404 = missing;

        // 0x01143030: native SessionMember defaults; known identity, color,
        // level and character-model item are bound by SceneRoster later.
        data.Member.ByteAt3 = 0xFF;
        data.Member.ValueAt88 = missing;
        data.Member.ValueAt108 = missing;
        data.Member.ValuesAt172[2] = 1;
        const auto playerId = static_cast<std::uint32_t>(character.CharacterId);
        data.Presets.PlayerId = playerId;
        // This generic assembler grants no items or pages. The MySQL provider
        // installs its loaded primary-weapon presets separately (E066).
        data.Inventory = std::move(inventory);
        return data;
    }
}
