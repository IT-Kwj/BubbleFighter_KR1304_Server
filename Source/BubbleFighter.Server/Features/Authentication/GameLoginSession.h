#pragma once

#include "InitialHandshake.h"
#include "MenuInitialization.h"
#include "Features/Characters/CharacterStore.h"
#include "Features/Lobby/LobbySession.h"
#include "Features/Lobby/PlazaRegistry.h"
#include "Features/Chat/SceneChat.h"
#include "Protocol/ShopEntryRequest.h"
#include "Protocol/ShopEntryResponse.h"
#include "Protocol/WeaponPresetChangeRequest.h"
#include "Features/Shop/ShopPurchase.h"
#include "Features/Shop/CashShop.h"
#include "Features/Inventory/ItemUse.h"
#include "Features/Practice/PracticeSession.h"
#include "Features/Rooms/RoomPacketQueue.h"

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace BubbleFighter::Server::Features::Authentication
{
    enum class GameLoginState
    {
        AwaitingInitialRecord, AwaitingTicket, AwaitingCharacterCreation, AwaitingCharacterProfile,
        LoginProfileSent, AwaitingMenuInitialization, MenuInitializationSent,
        PracticeLoading, PracticeCountdown, PracticePlaying, PracticeAwaitingReport, PracticeFinishing, PracticeResult, InRoom, Rejected
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
        using ShopPurchaseHandler = std::function<Shop::ShopPurchaseResult(const Characters::CharacterRecord&,
            const Protocol::ShopPurchaseRequest&, std::uint64_t observedAtUnixSeconds)>;
        using CashPickupHandler = std::function<Shop::CashPickupResult(const Characters::CharacterRecord&,
            const Protocol::CashInventoryPickupRequest&, std::uint64_t observedAtUnixSeconds)>;
        using WeaponPresetHandler = std::function<std::vector<Protocol::TcpPacket>(const Characters::CharacterRecord&,
            const Protocol::WeaponPresetChangeRequest&, std::uint64_t observedAtUnixSeconds)>;
        using ItemUseHandler = std::function<Inventory::ItemUseResult(const Characters::CharacterRecord&,
            const Protocol::ItemUseRequest&, std::uint64_t observedAtUnixSeconds)>;
        using PracticeClock = std::function<Practice::PracticeSession::Time()>;
        using PracticeSeedFactory = std::function<std::uint32_t()>;
        explicit GameLoginSession(TicketConsumer consumeTicket = {}, std::shared_ptr<Characters::CharacterStore> characters = {},
            MenuDataProvider menuData = {}, EquipmentChangeHandler equipmentChange = {}, ShopCatalogProvider shopCatalog = {},
            ShopPurchaseHandler shopPurchase = {}, CashPickupHandler cashPickup = {}, WeaponPresetHandler weaponPreset = {},
            ItemUseHandler itemUse = {}, PracticeClock practiceClock = Practice::PracticeSession::Clock::now,
            PracticeSeedFactory practiceSeed = Practice::PracticeSession::CreateRandomSeed,
            std::shared_ptr<Rooms::RoomRegistry> rooms = {}, std::shared_ptr<Lobby::PlazaRegistry> plazas = {});
        std::vector<Protocol::TcpPacket> Process(const Protocol::TcpPacket& packet);
        // Called by the transport even when no packet arrives. Uses monotonic time.
        std::vector<Protocol::TcpPacket> Poll();
        GameLoginState State() const noexcept;
        std::optional<std::uint64_t> AccountId() const noexcept;
        const std::optional<Characters::CharacterRecord>& Character() const noexcept;

    private:
        std::vector<Protocol::TcpPacket> ProcessCore(const Protocol::TcpPacket& packet);
        std::optional<std::vector<Protocol::TcpPacket>> ProcessRoom(const Protocol::TcpPacket& packet);
        std::vector<Protocol::TcpPacket> SendLoginProfile();
        std::vector<Protocol::TcpPacket> ReturnFromPractice();
        std::vector<Protocol::TcpPacket> EnterPlaza(MenuInitializationData data);
        void Reject() noexcept;
        void UpdatePracticeState() noexcept;
        bool CommerceInitialized() const noexcept;
        std::vector<Protocol::TcpPacket> ChangeInventory(const std::function<std::vector<Protocol::TcpPacket>()>& change);
        Chat::SceneChat chat_;
        InitialHandshake handshake_;
        TicketConsumer consumeTicket_;
        std::shared_ptr<Characters::CharacterStore> characters_;
        MenuDataProvider menuData_;
        EquipmentChangeHandler equipmentChange_;
        ShopCatalogProvider shopCatalog_;
        ShopPurchaseHandler shopPurchase_;
        CashPickupHandler cashPickup_;
        WeaponPresetHandler weaponPreset_;
        ItemUseHandler itemUse_;
        std::optional<std::uint64_t> shopRevisionSent_;
        GameLoginState state_ = GameLoginState::AwaitingInitialRecord;
        std::optional<std::uint64_t> accountId_;
        std::optional<Characters::CharacterRecord> character_;
        std::u16string checkedName_;
        std::optional<Lobby::LobbySession> lobby_;
        PracticeClock practiceClock_;
        PracticeSeedFactory practiceSeed_;
        std::optional<Practice::PracticeSession> practice_;
        std::shared_ptr<Rooms::RoomRegistry> rooms_;
        std::shared_ptr<Rooms::RoomPacketQueue> roomPackets_ = std::make_shared<Rooms::RoomPacketQueue>();
        Rooms::RoomLease room_;
        std::shared_ptr<Lobby::PlazaRegistry> plazas_;
        std::shared_ptr<Rooms::RoomPacketQueue> plazaPackets_ = std::make_shared<Rooms::RoomPacketQueue>();
        Lobby::PlazaLease plaza_;
    };
}
