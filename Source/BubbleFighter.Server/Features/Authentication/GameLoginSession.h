#pragma once

#include "InitialHandshake.h"
#include "MenuInitialization.h"
#include "Features/Characters/CharacterStore.h"
#include "Features/Lobby/LobbySession.h"
#include "Protocol/ShopEntryRequest.h"
#include "Protocol/ShopEntryResponse.h"

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace BubbleFighter::Server::Features::Authentication
{
    enum class GameLoginState
    {
        AwaitingInitialRecord, AwaitingTicket, AwaitingCharacterCreation, AwaitingCharacterProfile,
        LoginProfileSent, AwaitingMenuInitialization, MenuInitializationSent, Rejected
    };

    class GameLoginSession
    {
    public:
        using TicketConsumer = std::function<std::optional<std::uint64_t>(std::string_view)>;
        // Supplies trusted local initial state including loaded inventory.
        // ServerApplication installs LocalMenuData for configured MySQL;
        // callers without a provider retain the explicitly partial legacy path.
        // One observation time is shared by persistence, view and packet encoding.
        using MenuDataProvider = std::function<MenuInitializationData(
            const Characters::CharacterRecord&, std::uint64_t observedAtUnixSeconds)>;
        using EquipmentChangeHandler = std::function<std::vector<Protocol::TcpPacket>(
            const Characters::CharacterRecord&, std::uint64_t itemIdentifier, bool equip, std::uint64_t observedAtUnixSeconds)>;
        using ShopCatalogProvider = std::function<Protocol::ShopEntryResponse(const Protocol::ShopEntryRequest&,
            const Characters::CharacterRecord&, std::uint64_t observedAtUnixSeconds)>;
        explicit GameLoginSession(TicketConsumer consumeTicket = {}, std::shared_ptr<Characters::CharacterStore> characters = {},
            MenuDataProvider menuData = {}, EquipmentChangeHandler equipmentChange = {}, ShopCatalogProvider shopCatalog = {});
        std::vector<Protocol::TcpPacket> Process(const Protocol::TcpPacket& packet);
        GameLoginState State() const noexcept;
        std::optional<std::uint64_t> AccountId() const noexcept;
        const std::optional<Characters::CharacterRecord>& Character() const noexcept;

    private:
        std::vector<Protocol::TcpPacket> SendLoginProfile();
        InitialHandshake handshake_;
        TicketConsumer consumeTicket_;
        std::shared_ptr<Characters::CharacterStore> characters_;
        MenuDataProvider menuData_;
        EquipmentChangeHandler equipmentChange_;
        ShopCatalogProvider shopCatalog_;
        std::optional<std::uint64_t> shopRevisionSent_;
        GameLoginState state_ = GameLoginState::AwaitingInitialRecord;
        std::optional<std::uint64_t> accountId_;
        std::optional<Characters::CharacterRecord> character_;
        std::u16string checkedName_;
        std::optional<Lobby::LobbySession> lobby_;
    };
}
