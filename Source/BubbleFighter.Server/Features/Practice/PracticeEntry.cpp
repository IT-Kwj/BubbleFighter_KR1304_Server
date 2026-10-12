#include "PracticeEntry.h"
#include "Features/Scenes/SceneRoster.h"
#include "Protocol/GameLoadNotification.h"
#include "Protocol/GameTransportOptionsNotification.h"

#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Practice
{
    namespace
    {
        constexpr std::array<std::uint32_t, 13> Rules{
            3000, 6000, 4000, 11000, 45000, 142000, 20,
            240000, 241000, 240000, 240000, 15000, 15000};
        constexpr std::u16string_view MapKey = u"rel_Village07_Easy02";
        constexpr std::u16string_view MapName = u"泡泡村小河边休闲区";
    }

    void PracticeEntry::Validate(const Protocol::ClientToServerMessage0x006B& request)
    {
        // Do not let a client select arbitrary filesystem paths, rewards,
        // modes or rule values. See TutorialFlowResourceProfile.json (E068).
        if (request.Rules != Rules || request.RuleFlags != 16 || request.Directory != MapKey ||
            request.MapKey != MapKey || request.MapName != MapName || request.Difficulty != 1)
        {
            throw std::invalid_argument("Unsupported local practice map or rules.");
        }
    }

    std::vector<Protocol::TcpPacket> PracticeEntry::Prepare(
        const Protocol::ClientToServerMessage0x006B& request,
        const Characters::CharacterRecord& character, std::uint64_t authenticatedAccountId,
        Authentication::MenuInitializationData data, std::uint64_t observedAtUnixSeconds)
    {
        Validate(request);
        if (data.Scene.Descriptor.SessionType != 1 || data.Equipment.Key != character.CharacterId)
        { throw std::invalid_argument("Practice entry requires a fresh trusted menu snapshot."); }

        // Refresh validates inventory ownership, presets and packet capacity.
        // All packets are encoded before the caller changes its session state.
        auto refresh = Inventory::InventoryRefresh::Prepare(character, authenticatedAccountId,
            data.Inventory, data.Equipment, data.Presets, observedAtUnixSeconds);
        auto& descriptor = data.Scene.Descriptor;
        descriptor.SessionType = 2;
        descriptor.ValueAt4 = 3; // 94D7A0: Launch (not Play=4).
        auto& config = descriptor.Configuration;
        config.ValueAt0 = 0; // Explicit native baseline: no special session flags.
        config.ShortAt4 = 40; // Sign-extended into model +144, protected at context +1776.
        config.TextsAt44[0] = MapKey; // model +184
        config.TextsAt44[1] = MapKey; // model +208: directory
        config.TextsAt44[2] = MapName; // model +232: display name
        Scenes::SceneSlot player;
        player.Character = character;
        player.State = std::move(data.Member);
        player.Equipment = std::move(data.Equipment);
        auto scene = Scenes::SceneRoster::ComposePractice(std::move(data.Scene), player, authenticatedAccountId);

        Protocol::GameLoadNotification load;
        // 9E5EB7..9E5FF3 reorders {directory,name,key} into {key,directory,name}.
        load.TextsAt0 = {std::u16string{MapKey}, std::u16string{MapName}, std::u16string{MapKey}};
        load.ValueAt72 = 3;
        // Unknown flags and collections use an explicit local solo baseline.
        // Remaining unknown scalar fields retain explicit zero values. This
        // permits loading only; it does not authorize a match or grant rewards.
        std::vector<Protocol::TcpPacket> packets;
        packets.reserve(refresh.size() + 3);
        packets.push_back(scene.Encode());
        for (auto& packet : refresh) { packets.push_back(std::move(packet)); }
        // E082: explicitly select the connected main TCP path, without relay,
        // UDP or movement deltas, before map loading. This is local solo policy;
        // it does not imply that every gameplay message can safely be echoed.
        packets.push_back(Protocol::GameTransportOptionsNotification{0, 0, 0}.Encode());
        packets.push_back(load.Encode());
        return packets;
    }
}
