#include "GameLoginSession.h"
#include "LoginProfile.h"

#include "Features/Characters/CharacterCreation.h"
#include "Features/Parties/SoloParty.h"
#include "Features/Configuration/LocalDynamicConfiguration.h"
#include "Protocol/LoginRequest.h"
#include "Protocol/CreateUserInfoNotification.h"
#include "Protocol/CheckCreateUserNameRequest.h"
#include "Protocol/CheckCreateUserNameResponse.h"
#include "Protocol/CreateUserInfoRequest.h"
#include "Protocol/CreateUserInfoResponse.h"
#include "Protocol/ClientToServerMessage0x0021.h"
#include "Protocol/ClientToServerMessage0x0209.h"
#include "Protocol/ClientToServerMessage0x0069.h"
#include "Protocol/InventoryRefreshRequest.h"
#include "Protocol/EquipItemRequest.h"
#include "Protocol/UnequipItemRequest.h"
#include "Features/Inventory/InventoryRefresh.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace BubbleFighter::Server::Features::Authentication
{
    namespace
    {
        std::uint64_t InventoryObservationTime()
        {
            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            if (seconds < 0) { throw std::runtime_error("Server clock predates the Unix epoch."); }
            return static_cast<std::uint64_t>(seconds);
        }
    }

    GameLoginSession::GameLoginSession(TicketConsumer consumeTicket, std::shared_ptr<Characters::CharacterStore> characters,
        MenuDataProvider menuData, EquipmentChangeHandler equipmentChange, ShopCatalogProvider shopCatalog) :
        consumeTicket_(std::move(consumeTicket)), characters_(std::move(characters)),
        menuData_(std::move(menuData)), equipmentChange_(std::move(equipmentChange)), shopCatalog_(std::move(shopCatalog)) {}

    std::vector<Protocol::TcpPacket> GameLoginSession::Process(const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        try
        {
            if (state_ == GameLoginState::Rejected) { throw std::runtime_error("Game login session rejected."); }
            if (packet.MessageId == DynamicConfigRequest::MessageId)
            {
                if (state_ == GameLoginState::AwaitingInitialRecord)
                { throw std::runtime_error("Dynamic configuration before initial record."); }
                return {Configuration::LocalDynamicConfiguration::Respond(DynamicConfigRequest::Decode(packet)).Encode()};
            }
            if (packet.MessageId == LoginRequest::MessageId)
            {
                if (state_ != GameLoginState::AwaitingTicket) { throw std::runtime_error("Login request in invalid session state."); }
                if (!consumeTicket_ || !characters_) { throw std::runtime_error("Local game authentication is not configured."); }
                const auto request = LoginRequest::Decode(packet);
                if (request.NexonPassport.size() != 69) { throw std::runtime_error("Invalid local ticket."); }
                std::string ticket;
                for (const auto codeUnit : request.NexonPassport)
                {
                    if (codeUnit > 127) { throw std::runtime_error("Invalid local ticket."); }
                    ticket.push_back(static_cast<char>(codeUnit));
                }
                const auto account = consumeTicket_(ticket);
                if (!account || *account == 0) { throw std::runtime_error("Local ticket rejected."); }
                accountId_ = account;
                character_ = characters_->FindByAccount(*accountId_);
                if (character_)
                {
                    state_ = GameLoginState::AwaitingCharacterProfile;
                    return SendLoginProfile();
                }
                state_ = GameLoginState::AwaitingCharacterCreation;
                return {CreateUserInfoNotification{}.Encode()};
            }
            if (packet.MessageId == CheckCreateUserNameRequest::MessageId || packet.MessageId == CreateUserInfoRequest::MessageId)
            {
                if (!accountId_ || !characters_) { throw std::runtime_error("Character operation before authentication."); }
                if (state_ != GameLoginState::AwaitingCharacterCreation)
                { throw std::runtime_error("Character operation in invalid session state."); }
                Characters::CharacterCreation creation(*characters_);
                if (packet.MessageId == CheckCreateUserNameRequest::MessageId)
                {
                    const auto request = CheckCreateUserNameRequest::Decode(packet);
                    const auto result = creation.CheckName(request.CharacterName);
                    checkedName_ = result == 0 ? request.CharacterName : std::u16string{};
                    return {CheckCreateUserNameResponse{result, request.CharacterName}.Encode()};
                }
                const auto request = CreateUserInfoRequest::Decode(packet);
                if (checkedName_.empty() || checkedName_ != request.CharacterName) { return {CreateUserInfoResponse{2}.Encode()}; }
                const auto result = creation.Create(*accountId_, request);
                checkedName_.clear();
                if (result == 0)
                {
                    character_ = characters_->FindByAccount(*accountId_);
                    if (!character_) { throw std::runtime_error("Created character was not persisted."); }
                    state_ = GameLoginState::AwaitingCharacterProfile;
                }
                return {CreateUserInfoResponse{result}.Encode()};
            }
            if (packet.MessageId == ClientToServerMessage0x0021::MessageId)
            {
                if (!accountId_) { throw std::runtime_error("Creation follow-up before authentication."); }
                ClientToServerMessage0x0021::Decode(packet);
                if (state_ == GameLoginState::AwaitingCharacterProfile) { return SendLoginProfile(); }
                // E028: this is also reachable after failed creation. It cannot
                // grant identity or imply a successful persisted character.
                return {};
            }
            if (packet.MessageId == ClientToServerMessage0x0209::MessageId)
            {
                ClientToServerMessage0x0209::Decode(packet);
                if (!accountId_ || !character_ || (state_ != GameLoginState::LoginProfileSent &&
                    state_ != GameLoginState::AwaitingMenuInitialization && state_ != GameLoginState::MenuInitializationSent))
                { throw std::runtime_error("C521 before authenticated character profile."); }
                if (state_ != GameLoginState::AwaitingMenuInitialization) { return {}; }
                // E054 local policy: first observed common-UI initialization
                // signal after S3. This does not assign C521 a recovered request
                // meaning. Repeated UI entry cannot reload/reset menu state.
                const auto observedAt = InventoryObservationTime();
                auto responses = MenuInitialization::Prepare(*character_, *accountId_, menuData_(*character_, observedAt), observedAt);
                state_ = GameLoginState::MenuInitializationSent;
                return responses;
            }
            if (packet.MessageId == ClientToServerMessage0x006A::MessageId)
            {
                if ((state_ != GameLoginState::LoginProfileSent && state_ != GameLoginState::MenuInitializationSent) ||
                    !accountId_ || !character_ || !lobby_)
                { throw std::runtime_error("Lobby request before authenticated character initialization."); }
                const auto request = ClientToServerMessage0x006A::Decode(packet);
                if (menuData_)
                {
                    // Reload trusted local state; do not carry a partially
                    // initialized menu context into the plaza. S117 contains
                    // both equipment maps and the owned local member.
                    const auto observedAt = InventoryObservationTime();
                    auto data = menuData_(*character_, observedAt);
                    Scenes::SceneSlot localPlayer;
                    localPlayer.Character = *character_;
                    localPlayer.State = std::move(data.Member);
                    localPlayer.Equipment = std::move(data.Equipment);
                    return {lobby_->RequestEntry(request, std::move(data.Scene), std::move(localPlayer), *accountId_)};
                }
                return {lobby_->RequestEntry(request).Encode()};
            }
            if (packet.MessageId == ClientToServerMessage0x0069::MessageId)
            {
                ClientToServerMessage0x0069::Decode(packet);
                if (state_ != GameLoginState::MenuInitializationSent || !menuData_ ||
                    !accountId_ || !character_ || !lobby_)
                { throw std::runtime_error("Menu return before full authenticated initialization."); }
                // Local menu/plaza policy: return with an owned fresh snapshot.
                // S117 type 1 establishes the context before selecting stage 2.
                // Room, party and active-match leave semantics are not assigned
                // to this branch until those server states are implemented.
                const auto observedAt = InventoryObservationTime();
                auto responses = MenuInitialization::Prepare(*character_, *accountId_, menuData_(*character_, observedAt), observedAt);
                lobby_.emplace();
                return responses;
            }
            if (packet.MessageId == ShopEntryRequest::MessageId)
            {
                const auto request = ShopEntryRequest::Decode(packet);
                if (state_ != GameLoginState::MenuInitializationSent || !menuData_ || !shopCatalog_ ||
                    !accountId_ || !character_ || !lobby_ || character_->AccountId != *accountId_)
                { throw std::runtime_error("Shop entry before full authenticated initialization."); }
                auto response = shopCatalog_(request, *character_, InventoryObservationTime());
                if (!response.RefreshCatalog || response.CatalogRevision == 0)
                { throw std::runtime_error("Shop provider must supply a complete versioned snapshot."); }
                auto encoded = response.Encode(); // validate before acknowledging any cache hit
                if (shopRevisionSent_ == response.CatalogRevision && request.CatalogRevision == response.CatalogRevision)
                {
                    response.RefreshCatalog = false;
                    response.Items.clear();
                    response.Categories.clear();
                    encoded = response.Encode();
                }
                // A client-provided version cannot suppress the first snapshot
                // in this connection. Scene/menu state and inventory stay intact.
                shopRevisionSent_ = response.CatalogRevision;
                return {std::move(encoded)};
            }
            if (packet.MessageId == EquipItemRequest::MessageId || packet.MessageId == UnequipItemRequest::MessageId)
            {
                const bool equip = packet.MessageId == EquipItemRequest::MessageId;
                const auto item = equip ? EquipItemRequest::Decode(packet).ItemIdentifier : UnequipItemRequest::Decode(packet).ItemIdentifier;
                if (state_ != GameLoginState::MenuInitializationSent || !menuData_ || !equipmentChange_ ||
                    !accountId_ || !character_ || !lobby_ || character_->AccountId != *accountId_)
                { throw std::runtime_error("Equipment change before full authenticated initialization."); }
                // Installed storage prepares both updates before committing all
                // changed flags. No guessed success code, S3 or scene reset.
                return equipmentChange_(*character_, item, equip, InventoryObservationTime());
            }
            if (packet.MessageId == InventoryRefreshRequest::MessageId)
            {
                InventoryRefreshRequest::Decode(packet);
                if (state_ != GameLoginState::MenuInitializationSent || !menuData_ ||
                    !accountId_ || !character_ || !lobby_)
                { throw std::runtime_error("Inventory refresh before full authenticated initialization."); }
                // Reload persistence for every explicit refresh. Reuse the
                // trusted menu data source without replaying menu initialization
                // or changing the established plaza/menu context.
                const auto observedAt = InventoryObservationTime();
                const auto data = menuData_(*character_, observedAt);
                return Inventory::InventoryRefresh::Prepare(*character_, *accountId_, data.Inventory,
                    data.Equipment, data.Presets, observedAt);
            }
            auto responses = handshake_.Process(packet);
            if (handshake_.State() == InitialHandshakeState::Rejected) { state_ = GameLoginState::Rejected; }
            else if (state_ == GameLoginState::AwaitingInitialRecord && handshake_.State() == InitialHandshakeState::InitialRecordAccepted)
            { state_ = GameLoginState::AwaitingTicket; }
            return responses;
        }
        catch (...)
        {
            state_ = GameLoginState::Rejected;
            accountId_.reset();
            character_.reset();
            checkedName_.clear();
            lobby_.reset();
            shopRevisionSent_.reset();
            throw;
        }
    }

    std::vector<Protocol::TcpPacket> GameLoginSession::SendLoginProfile()
    {
        if (!accountId_ || !character_) { throw std::runtime_error("Missing authenticated character profile."); }
        auto packet = LoginProfile::Create(*accountId_, *character_, std::chrono::system_clock::now()).Encode();
        auto party = Parties::SoloParty::Create(*character_).Encode();
        lobby_.emplace();
        state_ = menuData_ ? GameLoginState::AwaitingMenuInitialization : GameLoginState::LoginProfileSent;
        return {std::move(packet), std::move(party)};
    }

    GameLoginState GameLoginSession::State() const noexcept { return state_; }
    std::optional<std::uint64_t> GameLoginSession::AccountId() const noexcept { return accountId_; }
    const std::optional<Characters::CharacterRecord>& GameLoginSession::Character() const noexcept { return character_; }
}
