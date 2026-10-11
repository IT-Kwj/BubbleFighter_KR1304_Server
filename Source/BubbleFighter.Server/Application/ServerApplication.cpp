#include "ServerApplication.h"
#include "LocalAccountCommand.h"

#include "Network/TcpServer.h"
#include "Features/Authentication/GameLoginSession.h"
#include "Features/Authentication/LocalAuthentication.h"
#include "Features/Authentication/LocalMenuData.h"
#include "Features/Shop/LocalShopCatalog.h"
#include "Persistence/MySqlAccountStore.h"
#include "Persistence/MySqlCharacterStore.h"
#include "Persistence/MySqlInventoryStore.h"

#include <charconv>
#include <chrono>
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
                      << "Usage: BubbleFighter.Server --listen-port <0..65535> [--once] [--database-config <path>] [--idle-timeout-seconds <1..600>]\n"
                      << "Development listener uses 127.0.0.1. Port 0 selects an available port.\n";
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
        bool once = false;
        unsigned int port = 0;
        unsigned int idleTimeoutSeconds = 30;
        bool hasIdleTimeout = false;
        std::filesystem::path databaseConfiguration;
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
            const auto createHandler = [databaseConfiguration]() -> Network::PacketHandler
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
                if (!databaseConfiguration.empty())
                {
                    menu = [databaseConfiguration](const Features::Characters::CharacterRecord& character, std::uint64_t observedAt)
                    {
                        Persistence::MySqlInventoryStore store(databaseConfiguration);
                        auto inventory = store.Load(character.AccountId, character.CharacterId, observedAt);
                        if (!inventory) { throw std::runtime_error("Owned character inventory is unavailable."); }
                        return Features::Authentication::LocalMenuData::Create(character, std::move(*inventory),
                            observedAt);
                    };
                    equipmentChange = [databaseConfiguration](const Features::Characters::CharacterRecord& character,
                        std::uint64_t itemIdentifier, bool equip, std::uint64_t observedAt)
                    {
                        Persistence::MySqlInventoryStore store(databaseConfiguration);
                        return store.ChangeEquipment(character, itemIdentifier, equip, observedAt).Packets;
                    };
                    shopCatalog = Features::Shop::LocalShopCatalog::Create;
                }
                return [session = Features::Authentication::GameLoginSession{std::move(consume), std::move(characters), std::move(menu), std::move(equipmentChange), std::move(shopCatalog)}](const Protocol::TcpPacket& packet) mutable
                {
                    const auto before = session.State();
                    auto response = session.Process(packet);
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
            };
            return Network::TcpServer{}.Run(static_cast<std::uint16_t>(port), once, createHandler, idleTimeoutSeconds);
        }
        catch (const std::exception& error)
        {
            std::cerr << "Server error: " << error.what() << '\n';
            return 1;
        }
    }
}
