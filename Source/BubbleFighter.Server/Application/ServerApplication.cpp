#include "ServerApplication.h"
#include "LocalAccountCommand.h"

#include "Network/TcpServer.h"
#include "Network/ConnectionHeartbeat.h"
#include "Features/Authentication/GameLoginSession.h"
#include "Features/Authentication/LocalAuthentication.h"
#include "Features/Authentication/LocalMenuData.h"
#include "Features/Shop/LocalShopCatalog.h"
#include "Persistence/MySqlAccountStore.h"
#include "Persistence/MySqlCharacterStore.h"
#include "Persistence/MySqlInventoryStore.h"
#include "Persistence/MySqlMatchStore.h"

#include <charconv>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <exception>
#include <iostream>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Application
{
    namespace
    {
        void PrintUsage()
        {
            std::cout << "Status: local ticket authentication and character persistence implemented; full game login pending.\n"
                      << "Usage: BubbleFighter.Server --listen-port <0..65535> [--listen-address <IPv4>] [--once] [--database-config <path>] [--idle-timeout-seconds <1..600>] [--match-rules <path>]\n"
                      << "Default listen address: 127.0.0.1. Use a local IPv4 address for LAN or 0.0.0.0 for all IPv4 interfaces. Port 0 selects an available port.\n";
            std::cout << "Local accounts: --create-account <username> --database-config <path>\n"
                      << "Local login: --local-login <username> --database-config <path> --ticket-file <new-path>\n"
                      << "Passwords are read from UTF-8 standard input with console echo disabled.\n";
        }
    }

    int ServerApplication::Run(int argc, char* argv[]) const
    {
        std::cout << "BubbleFighter.Server\n";
        if (LocalAccountCommand::IsRequested(argc, argv)) { return LocalAccountCommand{}.Run(argc, argv); }
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help"))
        {
            PrintUsage();
            return 0;
        }

        bool hasPort = false;
        bool hasListenAddress = false;
        std::string_view listenAddress = "127.0.0.1";
        bool once = false;
        unsigned int port = 0;
        unsigned int idleTimeoutSeconds = 30;
        bool hasIdleTimeout = false;
        std::filesystem::path databaseConfiguration;
        std::filesystem::path matchRulesPath;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument(argv[index]);
            if (argument == "--listen-port" && !hasPort && index + 1 < argc)
            {
                const std::string_view value(argv[++index]);
                const auto result = std::from_chars(value.data(), value.data() + value.size(), port);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || port > 65535)
                {
                    std::cerr << "Invalid listen port.\n";
                    return 2;
                }
                hasPort = true;
            }
            else if (argument == "--listen-address" && !hasListenAddress && index + 1 < argc)
            {
                listenAddress = argv[++index];
                try { Network::TcpServer::ParseListenAddress(listenAddress); }
                catch (const std::invalid_argument& error) { std::cerr << error.what() << '\n'; return 2; }
                hasListenAddress = true;
            }
            else if (argument == "--database-config" && databaseConfiguration.empty() && index + 1 < argc)
            {
                databaseConfiguration = argv[++index];
            }
            else if (argument == "--idle-timeout-seconds" && !hasIdleTimeout && index + 1 < argc)
            {
                const std::string_view value(argv[++index]);
                const auto result = std::from_chars(value.data(), value.data() + value.size(), idleTimeoutSeconds);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || idleTimeoutSeconds == 0 || idleTimeoutSeconds > 600)
                { std::cerr << "Invalid idle timeout.\n"; return 2; }
                hasIdleTimeout = true;
            }
            else if (argument == "--match-rules" && matchRulesPath.empty() && index + 1 < argc)
            { matchRulesPath = argv[++index]; }
            else if (argument == "--once" && !once)
            {
                once = true;
            }
            else
            {
                std::cerr << "Invalid or duplicate argument: " << argument << '\n';
                PrintUsage();
                return 2;
            }
        }
        if (!hasPort)
        {
            std::cerr << "--listen-port is required to start listening.\n";
            return 2;
        }
        try
        {
            const auto rules = matchRulesPath.empty() ? Features::Rooms::LocalMatchRules{} :
                Features::Rooms::LocalMatchRules::Load(matchRulesPath);
            Features::Rooms::RoomMatchSettler settler;
            if (!databaseConfiguration.empty())
            {
                // Fail before listening if the required receipt migration is absent.
                Persistence::MySqlMatchStore checkSchema(databaseConfiguration);
                settler = [databaseConfiguration](const Features::Rooms::RoomMatchRequest& request)
                { return Persistence::MySqlMatchStore(databaseConfiguration).Settle(request); };
            }
            const auto rooms = Features::Rooms::RoomRegistry::Create(512, {}, {}, std::move(settler), rules);
            // Match rewards outlive individual TCP handlers and even the last
            // room member. The worker holds no room-table lock during retries.
            std::jthread settlementWorker;
            if (!databaseConfiguration.empty())
            {
                settlementWorker = std::jthread([rooms, databaseConfiguration](std::stop_token stop)
                {
                    std::mutex waitMutex;
                    std::condition_variable_any wake;
                    std::unique_lock waitLock(waitMutex);
                    std::array<std::uint8_t, 16> cursor{};
                    auto recoverAt = std::chrono::steady_clock::time_point{};
                    bool recoveryDelayed = false;
                    while (!stop.stop_requested())
                    {
                        const auto now = std::chrono::steady_clock::now();
                        if (now >= recoverAt)
                        {
                            try
                            {
                                auto pending = Persistence::MySqlMatchStore(databaseConfiguration).LoadPending(cursor);
                                for (auto& request : pending) { rooms->RecoverSettlement(request); }
                                cursor = pending.empty() ? std::array<std::uint8_t, 16>{} : pending.back().MatchId;
                                recoveryDelayed = false;
                            }
                            catch (const std::exception&)
                            {
                                if (!recoveryDelayed) { std::cerr << "Pending match recovery scan delayed.\n"; }
                                recoveryDelayed = true;
                            }
                            recoverAt = now + std::chrono::seconds{2};
                        }
                        try { rooms->RetrySettlements(); }
                        catch (const std::exception&) { std::cerr << "Cannot poll pending match settlements.\n"; }
                        wake.wait_for(waitLock, stop, std::chrono::milliseconds{100}, [] { return false; });
                    }
                });
            }
            const auto plazas = std::make_shared<Features::Lobby::PlazaRegistry>();
            const auto createHandler = [databaseConfiguration, idleTimeoutSeconds, rooms, rules, plazas]() -> Network::ConnectionHandlers
            {
                Features::Authentication::GameLoginSession::TicketConsumer consume;
                if (!databaseConfiguration.empty())
                {
                    consume = [databaseConfiguration](std::string_view ticket)
                    {
                        Persistence::MySqlAccountStore store(databaseConfiguration);
                        return Features::Authentication::LocalAuthentication{store}.ConsumeTicket(ticket);
                    };
                }
                std::shared_ptr<Features::Characters::CharacterStore> characters;
                if (!databaseConfiguration.empty()) { characters = std::make_shared<Persistence::MySqlCharacterStore>(databaseConfiguration); }
                Features::Authentication::GameLoginSession::MenuDataProvider menu;
                Features::Authentication::GameLoginSession::EquipmentChangeHandler equipmentChange;
                Features::Authentication::GameLoginSession::ShopCatalogProvider shopCatalog;
                Features::Authentication::GameLoginSession::ShopPurchaseHandler shopPurchase;
                Features::Authentication::GameLoginSession::CashPickupHandler cashPickup;
                Features::Authentication::GameLoginSession::WeaponPresetHandler weaponPreset;
                Features::Authentication::GameLoginSession::ItemUseHandler itemUse;
                if (!databaseConfiguration.empty())
                {
                    menu = [databaseConfiguration](const Features::Characters::CharacterRecord& character, std::uint64_t observedAt)
                    {
                        Persistence::MySqlInventoryStore store(databaseConfiguration);
                        auto snapshot = store.LoadMenuSnapshot(character, observedAt);
                        if (!snapshot) { throw std::runtime_error("Character inventory is unavailable."); }
                        auto presets = snapshot->Presets.CreateNotification(character, snapshot->Inventory, observedAt);
                        auto data = Features::Authentication::LocalMenuData::Create(character, std::move(snapshot->Inventory), observedAt);
                        data.Presets = std::move(presets);
                        data.Cash = std::move(snapshot->Cash);
                        return data;
                    };
                    equipmentChange = [databaseConfiguration](const Features::Characters::CharacterRecord& character,
                        std::uint64_t itemIdentifier, bool equip, std::uint64_t observedAt)
                    {
                        Persistence::MySqlInventoryStore store(databaseConfiguration);
                        return store.ChangeEquipment(character, itemIdentifier, equip, observedAt).Packets;
                    };
                    shopCatalog = Features::Shop::LocalShopCatalog::Create;
                    shopPurchase = [databaseConfiguration](const Features::Characters::CharacterRecord& character,
                        const Protocol::ShopPurchaseRequest& request, std::uint64_t observedAt)
                    {
                        Persistence::MySqlInventoryStore store(databaseConfiguration);
                        return store.Purchase(character, request, observedAt);
                    };
                    cashPickup = [databaseConfiguration](const Features::Characters::CharacterRecord& character,
                        const Protocol::CashInventoryPickupRequest& request, std::uint64_t observedAt)
                    {
                        Persistence::MySqlInventoryStore store(databaseConfiguration);
                        return store.Pickup(character, request, observedAt);
                    };
                    weaponPreset = [databaseConfiguration](const Features::Characters::CharacterRecord& character,
                        const Protocol::WeaponPresetChangeRequest& request, std::uint64_t observedAt)
                    {
                        Persistence::MySqlInventoryStore store(databaseConfiguration);
                        return store.ChangeWeaponPreset(character, request, observedAt).Packets;
                    };
                    itemUse = [databaseConfiguration](const Features::Characters::CharacterRecord& character,
                        const Protocol::ItemUseRequest& request, std::uint64_t observedAt)
                    {
                        Persistence::MySqlInventoryStore store(databaseConfiguration);
                        return store.UseItem(character, request, observedAt);
                    };
                }
                const auto ownedSession = std::make_shared<Features::Authentication::GameLoginSession>(std::move(consume),
                    std::move(characters), std::move(menu), std::move(equipmentChange), std::move(shopCatalog),
                    std::move(shopPurchase), std::move(cashPickup), std::move(weaponPreset), std::move(itemUse),
                    Features::Practice::PracticeSession::Clock::now, Features::Practice::PracticeSession::CreateRandomSeed, rooms, plazas);
                Network::ConnectionHandlers handlers;
                handlers.OnPacket = [ownedSession, rules](const Protocol::TcpPacket& packet)
                {
                    auto& session = *ownedSession;
                    const auto before = session.State();
                    auto response = session.Process(packet);
                    rules.BindProfiles(response);
                    if (before != session.State() && session.State() == Features::Authentication::GameLoginState::AwaitingCharacterCreation)
                    {
                        std::cout << "Local ticket accepted; character creation required." << std::endl;
                    }
                    if (before != session.State() && session.State() == Features::Authentication::GameLoginState::AwaitingCharacterProfile)
                    {
                        std::cout << (packet.MessageId == 3 ? "Local ticket accepted; stored character profile pending." : "Character persisted; login profile pending.") << std::endl;
                        if (session.Character()) { std::cout << "CharacterType=" << session.Character()->CharacterType << " ColorIndex=" << session.Character()->ColorIndex << std::endl; }
                    }
                    if (before != session.State() && (session.State() == Features::Authentication::GameLoginState::LoginProfileSent ||
                        session.State() == Features::Authentication::GameLoginState::AwaitingMenuInitialization))
                    {
                        std::cout << (packet.MessageId == 3 ? "Local ticket accepted; stored character login profile sent." : "Persisted character login profile sent.") << std::endl;
                    }
                    if (before != session.State() && session.State() == Features::Authentication::GameLoginState::MenuInitializationSent)
                    { std::cout << "Owned character menu initialization sent." << std::endl; }
                    const bool close = session.State() == Features::Authentication::GameLoginState::Rejected;
                    return Network::PacketHandlingResult{std::move(response), close};
                };
                handlers.OnTick = [ownedSession, rules]
                {
                    auto response = ownedSession->Poll();
                    rules.BindProfiles(response);
                    return Network::PacketHandlingResult{std::move(response), false};
                };
                return Network::ConnectionHeartbeat::Wrap(std::move(handlers),
                    [ownedSession] { return ownedSession->AccountId().has_value(); }, idleTimeoutSeconds);
            };
            return Network::TcpServer{}.Run(static_cast<std::uint16_t>(port), once, createHandler,
                idleTimeoutSeconds, {}, {}, listenAddress);
        }
        catch (const std::exception& error)
        {
            std::cerr << "Server error: " << error.what() << '\n';
            return 1;
        }
    }
}
