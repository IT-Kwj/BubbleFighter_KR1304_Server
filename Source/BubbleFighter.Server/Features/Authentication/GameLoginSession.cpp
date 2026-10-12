#include "GameLoginSession.h"
#include "Features/Practice/PracticeEntry.h"
#include "Features/Rooms/RoomActions.h"
#include "Features/Rooms/RoomCreation.h"
#include "Protocol/LeaveRoomRequest.h"
#include "Protocol/ClientToServerMessage0x0270.h"
#include "Protocol/GameReportRequest.h"
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
#include "Protocol/CashBalanceRequest.h"
#include "Protocol/CashBalanceResponse.h"
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
        MenuDataProvider menuData, EquipmentChangeHandler equipmentChange, ShopCatalogProvider shopCatalog,
        ShopPurchaseHandler shopPurchase, CashPickupHandler cashPickup, WeaponPresetHandler weaponPreset, ItemUseHandler itemUse,
        PracticeClock practiceClock, PracticeSeedFactory practiceSeed, std::shared_ptr<Rooms::RoomRegistry> rooms, std::shared_ptr<Lobby::PlazaRegistry> plazas) :
        consumeTicket_(std::move(consumeTicket)), characters_(std::move(characters)),
        menuData_(std::move(menuData)), equipmentChange_(std::move(equipmentChange)), shopCatalog_(std::move(shopCatalog)),
        shopPurchase_(std::move(shopPurchase)), cashPickup_(std::move(cashPickup)), weaponPreset_(std::move(weaponPreset)),
        itemUse_(std::move(itemUse)), practiceClock_(std::move(practiceClock)), practiceSeed_(std::move(practiceSeed)),
        rooms_(std::move(rooms)), plazas_(std::move(plazas))
    {
        if (!practiceClock_ || !practiceSeed_) { throw std::invalid_argument("Missing practice clock or random seed source."); }
    }

    std::vector<Protocol::TcpPacket> GameLoginSession::Process(const Protocol::TcpPacket& packet)
    {
        try
        {
            if (room_) { room_.Poll(); if (auto current = room_.Character()) { character_ = std::move(current); } }
            auto result = plazaPackets_->Drain();
            for (auto& response : roomPackets_->Drain()) { result.push_back(std::move(response)); }
            auto direct = ProcessCore(packet);
            if (plaza_ && (packet.MessageId == Protocol::EquipItemRequest::MessageId ||
                packet.MessageId == Protocol::UnequipItemRequest::MessageId ||
                packet.MessageId == Protocol::WeaponPresetChangeRequest::MessageId ||
                packet.MessageId == Protocol::ItemUseRequest::MessageId))
            {
                auto data = menuData_(*character_, InventoryObservationTime());
                plaza_.Refresh({std::move(data.Member), *character_, std::move(data.Equipment)}, false);
            }
            for (auto& response : direct) { result.push_back(std::move(response)); }
            for (auto& response : roomPackets_->Drain()) { result.push_back(std::move(response)); }
            for (auto& response : plazaPackets_->Drain()) { result.push_back(std::move(response)); }
            return result;
        }
        catch (...) { Reject(); throw; }
    }

    std::vector<Protocol::TcpPacket> GameLoginSession::ProcessCore(const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        try
        {
            if (state_ == GameLoginState::Rejected) { throw std::runtime_error("Game login session rejected."); }
            if (auto responses = ProcessRoom(packet)) { return std::move(*responses); }
            if (packet.MessageId == ChatRequest::MessageId)
            {
                const auto request = ChatRequest::Decode(packet);
                if (!accountId_ || !character_ || !menuData_ || !lobby_ ||
                    (!CommerceInitialized() && !practice_))
                { throw std::runtime_error("Chat before authenticated initialization."); }
                if (!chat_.Admit(practiceClock_())) { return {}; }
                if (!Chat::SceneChat::ValidText(request.Text))
                { return {Chat::SceneChat::Notice(u"聊天内容须为 1 至 256 个有效字符。")}; }
                if (request.Channel > 1)
                { return {Chat::SceneChat::Notice(u"该聊天频道需要对应的社交系统，当前尚未开放。")}; }
                auto message = Chat::SceneChat::Prepare(request, *character_);
                if (!request.Expression.empty() && Chat::SceneChat::NeedsInventory(request.Text))
                {
                    const auto observedAt = InventoryObservationTime();
                    const auto data = menuData_(*character_, observedAt);
                    message = Chat::SceneChat::Prepare(request, *character_, &data.Inventory, observedAt);
                }
                if (room_) { room_.Chat(message); return {}; }
                if (plaza_) { plaza_.Chat(message); return {}; }
                if (request.Channel == 1)
                { return {Chat::SceneChat::Notice(u"队伍聊天仅在房间或对战中可用。")}; }
                return {Chat::SceneChat::Create(-3, character_->CharacterName, message).Encode()};
            }
            if (packet.MessageId == PlazaMovementRequest::MessageId)
            {
                const auto movement = PlazaMovementRequest::Decode(packet);
                if (plaza_) { plaza_.Move(movement); }
                return {}; // Late movement during a scene transition has no authority.
            }
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
                    state_ != GameLoginState::AwaitingMenuInitialization && state_ != GameLoginState::MenuInitializationSent &&
                    state_ != GameLoginState::PracticeLoading && state_ != GameLoginState::PracticeCountdown &&
                    state_ != GameLoginState::PracticePlaying && state_ != GameLoginState::PracticeAwaitingReport &&
                    state_ != GameLoginState::PracticeFinishing && state_ != GameLoginState::PracticeResult &&
                    state_ != GameLoginState::InRoom))
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
            if (packet.MessageId == ClientToServerMessage0x006B::MessageId)
            {
                const auto request = ClientToServerMessage0x006B::Decode(packet);
                if (state_ != GameLoginState::MenuInitializationSent || !accountId_ || !character_ ||
                    !menuData_ || !lobby_ || lobby_->State() != Lobby::LobbyEntryState::AwaitingRequest)
                { throw std::runtime_error("Practice entry outside fully initialized menu."); }
                Practice::PracticeEntry::Validate(request); // Reject before loading persistence.
                const auto observedAt = InventoryObservationTime();
                auto responses = Practice::PracticeEntry::Prepare(request, *character_, *accountId_,
                    menuData_(*character_, observedAt), observedAt);
                practice_.emplace(practiceClock_(), practiceSeed_());
                state_ = GameLoginState::PracticeLoading;
                return responses;
            }
            if (packet.MessageId == ClientToServerMessage0x0243::MessageId ||
                packet.MessageId == ClientToServerMessage0x025F::MessageId)
            {
                if (!accountId_ || !character_ || !practice_)
                { throw std::runtime_error("Loading data without an owned practice scene."); }
                const auto now = practiceClock_();
                if (packet.MessageId == ClientToServerMessage0x0243::MessageId)
                { practice_->SubmitLoadingData(ClientToServerMessage0x0243::Decode(packet), now); }
                else { practice_->SubmitLoadingActor(ClientToServerMessage0x025F::Decode(packet), now); }
                return {}; // No inferred success reply or persistence side effect.
            }
            if (packet.MessageId == GameLoadingCompleteRequest::MessageId)
            {
                const auto report = GameLoadingCompleteRequest::Decode(packet);
                if (!accountId_ || !character_ || !practice_)
                { throw std::runtime_error("Unsupported or out-of-order practice loading report."); }
                auto responses = practice_->LoadingComplete(report, practiceClock_());
                UpdatePracticeState();
                return responses;
            }
            if (packet.MessageId == ClientToServerMessage0x0270::MessageId)
            {
                const auto request = ClientToServerMessage0x0270::Decode(packet);
                if (state_ != GameLoginState::PracticePlaying || !practice_ || request.ValueAt0 != 1 || request.ValueAt4 != 0)
                { throw std::runtime_error("Unsupported training exit or invalid practice state."); }
                return ReturnFromPractice();
            }
            if (packet.MessageId == ClientToServerMessage0x0260::MessageId ||
                packet.MessageId == ClientToServerMessage0x07ED::MessageId)
            {
                if (!accountId_ || !character_ || !practice_)
                { throw std::runtime_error("Action report without an owned practice scene."); }
                if (packet.MessageId == ClientToServerMessage0x0260::MessageId)
                { practice_->SubmitActionReport(ClientToServerMessage0x0260::Decode(packet), practiceClock_()); }
                else
                { practice_->SubmitActionReport(ClientToServerMessage0x07ED::Decode(packet), practiceClock_()); }
                return {}; // No guessed acknowledgement, score, task reset or reward.
            }
            if (packet.MessageId == GameMovementNotification::MessageId)
            {
                if (!accountId_ || !character_ || !practice_)
                { throw std::runtime_error("Movement without an owned practice scene."); }
                practice_->SubmitMovement(GameMovementNotification::Decode(packet), practiceClock_());
                return {};
            }
            if (packet.MessageId == GameShotNotification::MessageId ||
                packet.MessageId == GameExtendedShotNotification::MessageId)
            {
                if (!accountId_ || !character_ || !practice_)
                { throw std::runtime_error("Shot without an owned practice scene."); }
                if (packet.MessageId == GameShotNotification::MessageId)
                { return practice_->SubmitShot(GameShotNotification::Decode(packet), practiceClock_()); }
                return practice_->SubmitShot(GameExtendedShotNotification::Decode(packet), practiceClock_());
            }
            if (packet.MessageId == GameActorDeathNotification::MessageId)
            {
                if (!accountId_ || !character_ || !practice_)
                { throw std::runtime_error("Actor death without an owned practice scene."); }
                return practice_->ReportActorDeath(GameActorDeathNotification::Decode(packet), practiceClock_());
            }
            if (packet.MessageId == GameActorRespawnWaitNotification::MessageId ||
                packet.MessageId == GameRespawnRequest::MessageId ||
                packet.MessageId == GameRespawnCompletionNotification::MessageId)
            {
                if (!accountId_ || !character_ || !practice_)
                { throw std::runtime_error("Respawn message without an owned practice scene."); }
                if (packet.MessageId == GameActorRespawnWaitNotification::MessageId)
                { return practice_->ReportRespawnWait(GameActorRespawnWaitNotification::Decode(packet), practiceClock_()); }
                if (packet.MessageId == GameRespawnRequest::MessageId)
                { return practice_->RequestRespawn(GameRespawnRequest::Decode(packet), practiceClock_()); }
                return practice_->CompleteRespawn(GameRespawnCompletionNotification::Decode(packet), practiceClock_());
            }
            if (packet.MessageId == GameReportRequest::MessageId)
            {
                const auto report = GameReportRequest::Decode(packet);
                if (!practice_ || !accountId_ || !character_)
                { throw std::runtime_error("Game report without an owned practice scene."); }
                practice_->SubmitReport(report, practiceClock_());
                UpdatePracticeState();
                return {}; // Poll prepares the local practice result; no database writes.
            }
            if (packet.MessageId == ClientToServerMessage0x006A::MessageId)
            {
                if ((state_ != GameLoginState::LoginProfileSent && state_ != GameLoginState::MenuInitializationSent && !practice_) ||
                    !accountId_ || !character_ || !lobby_)
                { throw std::runtime_error("Lobby request before authenticated character initialization."); }
                ClientToServerMessage0x006A::Decode(packet);
                if (menuData_)
                {
                    // Reload trusted local state; do not carry a partially
                    // initialized menu context into the plaza. S117 contains
                    // both equipment maps and the owned local member.
                    const auto observedAt = InventoryObservationTime();
                    auto data = menuData_(*character_, observedAt);
                    auto packets = EnterPlaza(std::move(data));
                    practice_.reset(); // S117 type 3 replaces the game model and selects LobbyStage.
                    state_ = GameLoginState::MenuInitializationSent;
                    return packets;
                }
                return {lobby_->RequestEntry({}).Encode()};
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
                plaza_.Release(); plazaPackets_->Clear();
                lobby_.emplace();
                return responses;
            }
            if (packet.MessageId == ShopEntryRequest::MessageId)
            {
                const auto request = ShopEntryRequest::Decode(packet);
                if (!CommerceInitialized() || !menuData_ || !shopCatalog_ ||
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
            if (packet.MessageId == ShopPurchaseRequest::MessageId)
            {
                const auto request = ShopPurchaseRequest::Decode(packet);
                if (!CommerceInitialized() || !menuData_ || !shopPurchase_ ||
                    !accountId_ || !character_ || !lobby_ || character_->AccountId != *accountId_)
                { throw std::runtime_error("Purchase before full authenticated initialization."); }
                auto result = shopPurchase_(*character_, request, InventoryObservationTime());
                if (result.Character.AccountId != *accountId_ || result.Character.CharacterId != character_->CharacterId)
                { throw std::runtime_error("Purchase provider changed authenticated identity."); }
                // The provider returns only after a complete commit or refusal.
                // Keep the latest trusted balances for later menu/plaza reloads.
                character_ = std::move(result.Character);
                if (room_) { room_.RefreshCharacter(*character_); }
                return std::move(result.Packets);
            }
            if (packet.MessageId == CashBalanceRequest::MessageId)
            {
                CashBalanceRequest::Decode(packet);
                if ((state_ != GameLoginState::MenuInitializationSent && !(state_ == GameLoginState::InRoom && room_)) ||
                    !menuData_ || !characters_ ||
                    !accountId_ || !character_ || !lobby_ || character_->AccountId != *accountId_)
                { throw std::runtime_error("Cash balance query before full authenticated initialization."); }
                // E090: the native room UI also sends this read-only query.
                // Reload on every request. A payment on another connection may
                // have changed Cash since this session's last character read.
                auto current = characters_->FindByAccount(*accountId_);
                if (!current || current->AccountId != *accountId_ || current->CharacterId != character_->CharacterId ||
                    current->CharacterName != character_->CharacterName || current->CharacterType != character_->CharacterType ||
                    current->ColorIndex != character_->ColorIndex || !current->Statistics.IsValid())
                { throw std::runtime_error("Cash balance source no longer matches the authenticated character."); }
                // The local service has one Cash balance. Unknown fields use
                // an explicit zero baseline, not recovered original defaults
                // or a made-up success code. No balance/item is granted here.
                auto response = CashBalanceResponse{0, current->Statistics.Cash, 0}.Encode();
                character_ = std::move(current);
                if (room_) { room_.RefreshCharacter(*character_); }
                return {std::move(response)};
            }
            if (packet.MessageId == CashInventoryPickupRequest::MessageId)
            {
                const auto request = CashInventoryPickupRequest::Decode(packet);
                if (!CommerceInitialized() || !menuData_ || !cashPickup_ ||
                    !accountId_ || !character_ || !lobby_ || character_->AccountId != *accountId_)
                { throw std::runtime_error("Cash pickup before full authenticated initialization."); }
                auto result = cashPickup_(*character_, request, InventoryObservationTime());
                if (result.Character.AccountId != *accountId_ || result.Character.CharacterId != character_->CharacterId)
                { throw std::runtime_error("Cash pickup provider changed authenticated identity."); }
                character_ = std::move(result.Character);
                if (room_) { room_.RefreshCharacter(*character_); }
                return std::move(result.Packets);
            }
            if (packet.MessageId == ItemUseRequest::MessageId)
            {
                const auto request = ItemUseRequest::Decode(packet);
                if (!CommerceInitialized() || !menuData_ || !itemUse_ ||
                    !accountId_ || !character_ || !lobby_ || character_->AccountId != *accountId_)
                { throw std::runtime_error("Item use before full authenticated initialization."); }
                return ChangeInventory([&]
                {
                    auto result = itemUse_(*character_, request, InventoryObservationTime());
                    if (result.Character.AccountId != *accountId_ || result.Character.CharacterId != character_->CharacterId ||
                        result.Character.CharacterType != character_->CharacterType || result.Character.ColorIndex != character_->ColorIndex ||
                        !result.Character.Statistics.IsValid() || Characters::CharacterCreation::ValidateName(result.Character.CharacterName) != 0)
                    { throw std::runtime_error("Item use provider changed authenticated identity."); }
                    character_ = std::move(result.Character);
                    return std::move(result.Packets);
                });
            }
            if (packet.MessageId == WeaponPresetChangeRequest::MessageId)
            {
                const auto request = WeaponPresetChangeRequest::Decode(packet);
                if (!CommerceInitialized() || !menuData_ || !weaponPreset_ ||
                    !accountId_ || !character_ || !lobby_ || character_->AccountId != *accountId_ ||
                    request.ObjectId != WeaponPresetChangeRequest::MenuObjectId)
                { throw std::runtime_error("Weapon preset change outside initialized menu/plaza."); }
                return ChangeInventory([&] { return weaponPreset_(*character_, request, InventoryObservationTime()); });
            }
            if (packet.MessageId == EquipItemRequest::MessageId || packet.MessageId == UnequipItemRequest::MessageId)
            {
                const bool equip = packet.MessageId == EquipItemRequest::MessageId;
                const auto item = equip ? EquipItemRequest::Decode(packet).ItemIdentifier : UnequipItemRequest::Decode(packet).ItemIdentifier;
                if (!CommerceInitialized() || !menuData_ || !equipmentChange_ ||
                    !accountId_ || !character_ || !lobby_ || character_->AccountId != *accountId_)
                { throw std::runtime_error("Equipment change before full authenticated initialization."); }
                // Installed storage prepares both updates before committing all
                // changed flags. No guessed success code, S3 or scene reset.
                return ChangeInventory([&] { return equipmentChange_(*character_, item, equip, InventoryObservationTime()); });
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
                auto packets = Inventory::InventoryRefresh::Prepare(*character_, *accountId_, data.Inventory,
                    data.Equipment, data.Presets, observedAt);
                if (data.Cash)
                {
                    if (data.Cash->CharacterId != character_->CharacterId) { throw std::runtime_error("Refresh cash owner mismatch."); }
                    packets.push_back(data.Cash->CreateNotification(*accountId_).Encode());
                }
                return packets;
            }
            auto responses = handshake_.Process(packet);
            if (handshake_.State() == InitialHandshakeState::Rejected) { state_ = GameLoginState::Rejected; }
            else if (state_ == GameLoginState::AwaitingInitialRecord && handshake_.State() == InitialHandshakeState::InitialRecordAccepted)
            { state_ = GameLoginState::AwaitingTicket; }
            return responses;
        }
        catch (...)
        {
            Reject();
            throw;
        }
    }

    std::vector<Protocol::TcpPacket> GameLoginSession::ReturnFromPractice()
    {
        if (!practice_ || !accountId_ || !character_ || !menuData_ || !lobby_)
        { throw std::runtime_error("Missing owned practice context."); }
        const auto observedAt = InventoryObservationTime();
        auto packets = MenuInitialization::Prepare(*character_, *accountId_, menuData_(*character_, observedAt), observedAt);
        practice_.reset();
        lobby_.emplace();
        state_ = GameLoginState::MenuInitializationSent;
        return packets;
    }

    std::vector<Protocol::TcpPacket> GameLoginSession::Poll()
    {
        try
        {
            if (state_ == GameLoginState::Rejected) { throw std::runtime_error("Game login session rejected."); }
            if (room_)
            {
                room_.Poll();
                if (auto current = room_.Character()) { character_ = std::move(current); }
                return roomPackets_->Drain();
            }
            if (!practice_) { return plazaPackets_->Drain(); }
            const auto now = practiceClock_();
            if (practice_->Expired(now)) { return ReturnFromPractice(); }
            auto packets = practice_->Poll(now);
            UpdatePracticeState();
            return packets;
        }
        catch (...) { Reject(); throw; }
    }

    std::vector<Protocol::TcpPacket> GameLoginSession::EnterPlaza(MenuInitializationData data)
    {
        Scenes::SceneSlot local{std::move(data.Member), *character_, std::move(data.Equipment)};
        if (!plazas_) { return {lobby_->RequestEntry({}, std::move(data.Scene), std::move(local), *accountId_)}; }
        if (plaza_) { plaza_.Refresh(std::move(local), true); }
        else { plaza_ = plazas_->Enter(std::move(data.Scene), std::move(local), *accountId_, plazaPackets_->Sink()); }
        lobby_->SnapshotPublished();
        return {}; // Ordered queue holds S117 before any subsequent plaza event.
    }

    void GameLoginSession::Reject() noexcept
    {
        room_.Release();
        plaza_.Release();
        state_ = GameLoginState::Rejected;
        accountId_.reset();
        character_.reset();
        checkedName_.clear();
        lobby_.reset();
        shopRevisionSent_.reset();
        practice_.reset();
    }

    void GameLoginSession::UpdatePracticeState() noexcept
    {
        if (!practice_) { return; }
        switch (practice_->Phase())
        {
        case Practice::PracticePhase::Loading: state_ = GameLoginState::PracticeLoading; break;
        case Practice::PracticePhase::Countdown: state_ = GameLoginState::PracticeCountdown; break;
        case Practice::PracticePhase::Playing: state_ = GameLoginState::PracticePlaying; break;
        case Practice::PracticePhase::AwaitingReport: state_ = GameLoginState::PracticeAwaitingReport; break;
        case Practice::PracticePhase::Finishing: state_ = GameLoginState::PracticeFinishing; break;
        case Practice::PracticePhase::Result: state_ = GameLoginState::PracticeResult; break;
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
    bool GameLoginSession::CommerceInitialized() const noexcept
    { return state_ == GameLoginState::MenuInitializationSent || (state_ == GameLoginState::InRoom && room_); }
    std::vector<Protocol::TcpPacket> GameLoginSession::ChangeInventory(const std::function<std::vector<Protocol::TcpPacket>()>& change)
    {
        if (!room_) { return change(); }
        std::vector<Protocol::TcpPacket> result;
        if (!room_.ChangeInventory([&]
        {
            result = change();
            auto data = menuData_(*character_, InventoryObservationTime());
            return Rooms::RoomInventoryChange{*character_, std::move(data.Equipment)};
        })) { return {Chat::SceneChat::Notice(u"对战进行中，请返回等待房间后更换装备或使用道具。")}; }
        return result;
    }
    std::optional<std::uint64_t> GameLoginSession::AccountId() const noexcept { return accountId_; }
    const std::optional<Characters::CharacterRecord>& GameLoginSession::Character() const noexcept { return character_; }
}

namespace BubbleFighter::Server::Features::Authentication
{
    std::optional<std::vector<Protocol::TcpPacket>> GameLoginSession::ProcessRoom(const Protocol::TcpPacket& packet)
    {
        using namespace Protocol;
        using namespace Rooms;
        const bool entry = packet.MessageId == RoomListRequest::MessageId || packet.MessageId == CreateRoomRequest::MessageId ||
            packet.MessageId == JoinRoomRequest::MessageId;
        const bool roomAction = packet.MessageId == RoomReadyRequest::MessageId || packet.MessageId == StartGameRequest::MessageId ||
            packet.MessageId == LeaveRoomRequest::MessageId;
        if (!entry && !roomAction && !room_) { return std::nullopt; }
        if (!entry && !roomAction && room_ && !RoomGameSession::Handles(packet.MessageId) &&
            packet.MessageId != ClientToServerMessage0x0270::MessageId && packet.MessageId != InventoryRefreshRequest::MessageId)
        { return std::nullopt; }
        if (!rooms_ || !accountId_ || !character_ || !menuData_ || !lobby_)
        { throw std::runtime_error("Room operation before authenticated initialization."); }
        const auto load = [&]
        {
            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if (seconds < 0) { throw std::runtime_error("Invalid room observation time."); }
            return std::pair{static_cast<std::uint64_t>(seconds), menuData_(*character_, static_cast<std::uint64_t>(seconds))};
        };
        if (entry)
        {
            if (room_ || state_ != GameLoginState::MenuInitializationSent || lobby_->State() != Lobby::LobbyEntryState::SnapshotSent)
            { throw std::runtime_error("Room entry outside the initialized plaza."); }
            if (packet.MessageId == RoomListRequest::MessageId)
            { return std::vector<TcpPacket>{RoomActions::List(*rooms_, RoomListRequest::Decode(packet)).Encode()}; }
            if (packet.MessageId == CreateRoomRequest::MessageId)
            {
                const auto request = CreateRoomRequest::Decode(packet);
                auto [observedAt, data] = load();
                (void)observedAt;
                RoomCreationPolicy policy{{RoomCreation::StandardPvpMode}, {0},
                    RoomCreation::DialogTitleCodeUnits, RoomCreation::DialogPasswordCodeUnits};
                auto outcome = RoomCreation::Create(*rooms_, policy, request,
                    {*accountId_, *character_, std::move(data.Member), std::move(data.Equipment), roomPackets_->Sink()});
                if (outcome.Room) { plaza_.Release(); plazaPackets_->Clear();
                    room_ = std::move(outcome.Room); state_ = GameLoginState::InRoom; }
                return outcome.Responses;
            }
            const auto request = JoinRoomRequest::Decode(packet);
            auto [observedAt, data] = load();
            (void)observedAt;
            auto outcome = RoomActions::Join(*rooms_, request,
                {*accountId_, *character_, std::move(data.Member), std::move(data.Equipment), roomPackets_->Sink()});
            if (outcome.Room) { plaza_.Release(); plazaPackets_->Clear();
                    room_ = std::move(outcome.Room); state_ = GameLoginState::InRoom; }
            return outcome.Responses;
        }
        if (!room_ || state_ != GameLoginState::InRoom) { throw std::runtime_error("Room action outside an owned room."); }
        if (packet.MessageId == RoomReadyRequest::MessageId)
        { RoomActions::SetReady(room_, RoomReadyRequest::Decode(packet)); }
        else if (packet.MessageId == StartGameRequest::MessageId)
        { RoomActions::Start(room_, RoomPlayPolicy{}, StartGameRequest::Decode(packet)); }
        else if (packet.MessageId == LeaveRoomRequest::MessageId)
        {
            LeaveRoomRequest::Decode(packet);
            auto [observedAt, data] = load();
            (void)observedAt;
            room_.Release();
            roomPackets_->Clear();
            auto responses = EnterPlaza(std::move(data));
            state_ = GameLoginState::MenuInitializationSent;
            return responses;
        }
        else if (packet.MessageId == InventoryRefreshRequest::MessageId)
        {
            InventoryRefreshRequest::Decode(packet);
            auto [observedAt, data] = load();
            auto packets = Inventory::InventoryRefresh::Prepare(*character_, *accountId_, data.Inventory, data.Equipment, data.Presets, observedAt);
            if (data.Cash)
            {
                if (data.Cash->CharacterId != character_->CharacterId) { throw std::runtime_error("Room cash owner mismatch."); }
                packets.push_back(data.Cash->CreateNotification(*accountId_).Encode());
            }
            return packets;
        }
        else if (packet.MessageId == ClientToServerMessage0x0270::MessageId)
        {
            const auto request = ClientToServerMessage0x0270::Decode(packet);
            if (request.ValueAt0 != 1 || request.ValueAt4 != 0) { throw std::runtime_error("Unsupported room game exit."); }
            room_.ReturnToWaiting();
        }
        else { room_.ProcessGame(packet); }
        return std::vector<TcpPacket>{};
    }
}
