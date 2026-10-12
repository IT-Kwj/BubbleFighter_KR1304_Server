#include "SceneChat.h"
#include "ChatExpressionData.h"
#include "Features/Inventory/ItemCatalog.h"
#include <algorithm>
#include <stdexcept>
namespace BubbleFighter::Server::Features::Chat
{
    bool SceneChat::ValidText(std::u16string_view text, std::size_t maximum) noexcept
    {
        if (text.empty() || text.size() > maximum) { return false; }
        bool visible = false;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const auto c = text[i];
            if (c < 0x20 || c == 0x7F || c == 0x2028 || c == 0x2029 || (c >= 0x202A && c <= 0x202E) ||
                (c >= 0x2066 && c <= 0x2069)) { return false; }
            if (c >= 0xD800 && c <= 0xDBFF)
            { if (++i == text.size() || text[i] < 0xDC00 || text[i] > 0xDFFF) { return false; } }
            else if (c >= 0xDC00 && c <= 0xDFFF) { return false; }
            if (c != u' ' && c != 0x3000) { visible = true; }
        }
        return visible;
    }
    bool SceneChat::NeedsInventory(std::u16string_view text) noexcept
    {
        return std::any_of(ExpressionData::Entries.begin(), ExpressionData::Entries.end(), [&](const auto& entry)
        { return entry.GrantCount != 0 && text.find(entry.Keyword) != text.npos; });
    }
    SceneChat::Message SceneChat::Prepare(const Protocol::ChatRequest& request, const Characters::CharacterRecord& character,
        const Inventory::CharacterInventory* inventory, std::uint64_t observedAtUnixSeconds)
    {
        if (request.Channel > 1 || !ValidText(request.Text)) { throw std::invalid_argument("Unsupported scene chat."); }
        // 99F1C0/9246B0 can intentionally suppress an otherwise matching
        // keyword action. Empty means text only; do not reconstruct a motion.
        if (request.Expression.empty()) { return {request.Channel, request.Text, {}}; }
        if (inventory)
        {
            if (inventory->AccountId != character.AccountId || inventory->CharacterId != character.CharacterId)
            { throw std::invalid_argument("Chat expression inventory owner mismatch."); }
            (void)inventory->ToRecords(character.AccountId, observedAtUnixSeconds);
        }
        const auto owned = [&](std::uint32_t item)
        {
            if (!inventory) { return false; }
            const auto* definition = Inventory::ItemCatalog::Find(item);
            return definition && std::any_of(inventory->Slots.begin(), inventory->Slots.end(), [&](const auto& slot)
            {
                if (slot.ItemId != item || slot.Usage != definition->Usage) { return false; }
                return slot.Usage == Inventory::InventoryUsage::Duration ?
                    (!slot.ExpiresAtUnixSeconds || *slot.ExpiresAtUnixSeconds > observedAtUnixSeconds) : slot.Quantity != 0;
            });
        };
        for (const auto& entry : ExpressionData::Entries)
        {
            if (entry.Motion != request.Expression || request.Text.find(entry.Keyword) == request.Text.npos) { continue; }
            bool permitted = entry.GrantCount == 0;
            for (std::size_t n = 0; n < entry.GrantCount; ++n)
            {
                const auto& grant = ExpressionData::Grants[entry.GrantOffset + n];
                if (grant.Character == character.CharacterType && owned(grant.Item)) { permitted = true; break; }
            }
            if (!permitted) { continue; }
            // Preserve the native unordered-table choice, but only when its
            // keyword and character/item grant match trusted server state.
            return {request.Channel, request.Text, std::u16string(entry.Motion)};
        }
        return {request.Channel, request.Text, {}};
    }
    Protocol::ChatNotification SceneChat::Create(std::int16_t slot, std::u16string_view trustedName,
        const Message& message)
    {
        if (message.Channel > 1 || !ValidText(message.Text)) { throw std::invalid_argument("Unsupported scene chat."); }
        Protocol::ChatNotification m; m.Slot = slot; m.Sender = trustedName; m.Channel = message.Channel;
        m.Text = message.Text;
        if (slot >= 0)
        {
            // BFCLIENT02 9EAC70 checks marker "e" and plays the optional motion
            // from S509 +96. C502's second string is not S509's marker field.
            m.Expression = u"e";
            m.StringAt96 = message.Motion;
        }
        return m;
    }
    Protocol::TcpPacket SceneChat::Notice(std::u16string_view text)
    { Protocol::ChatNotification m; m.Sender = u"系统"; m.Text = text; return m.Encode(); }
    bool SceneChat::Admit(Time now)
    {
        using namespace std::chrono_literals;
        if (!sent_.empty() && now < sent_.back()) { return false; }
        while (!sent_.empty() && now - sent_.front() >= 3s) { sent_.pop_front(); }
        if (sent_.size() >= 5) { return false; }
        sent_.push_back(now); return true;
    }
}
