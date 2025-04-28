﻿/*
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// \addtogroup Trinityd Trinity Daemon
/// @{
/// \file

#include "Common.h"
#include "DatabaseEnv.h"
#include "IoContext.h"
#include "AsyncAcceptor.h"
#include "RASession.h"
#include "Configuration/Config.h"
#include "OpenSSLCrypto.h"
#include "ProcessPriority.h"
#include "BigNumber.h"
#include "World.h"
#include "MapManager.h"
#include "InstanceSaveMgr.h"
#include "ObjectAccessor.h"
#include "ScriptMgr.h"
#include "ScriptReloadMgr.h"
#include "ScriptLoader.h"
#include "OutdoorPvP/OutdoorPvPMgr.h"
#include "BattlegroundMgr.h"
#include "TCSoap.h"
#include "CliRunnable.h"
#include "GitRevision.h"
#include "WorldSocket.h"
#include "WorldSocketMgr.h"
#include "RealmList.h"
#include "DatabaseLoader.h"
#include "AppenderDB.h"
#include "Metric.h"
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include <google/protobuf/stubs/common.h>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/algorithm/string/replace.hpp>

using namespace boost::program_options;
namespace fs = boost::filesystem;

#ifndef _TRINITY_CORE_CONFIG
    #define _TRINITY_CORE_CONFIG  "worldserver.conf"
#endif

#define WORLD_SLEEP_CONST 50

#ifdef _WIN32
#include "ServiceWin32.h"
#include <boost/asio/signal_set.hpp>
char serviceName[] = "worldserver";
char serviceLongName[] = "TrinityCore world service";
char serviceDescription[] = "TrinityCore World of Warcraft emulator world service";
/*
 * -1 - not in service mode
 *  0 - stopped
 *  1 - running
 *  2 - paused
 */
//Stitch INFO: int m_ServiceStatus = -1; =1
int m_ServiceStatus = -1;
#endif

Trinity::Asio::IoContext ioContext;
uint32 _worldLoopCounter(0);
uint32 _lastChangeMsTime(0);
uint32 _maxCoreStuckTimeInMs(0);

void SignalHandler(const boost::system::error_code& error, int signalNumber);
AsyncAcceptor* StartRaSocketAcceptor(Trinity::Asio::IoContext& ioContext);
bool StartDB();
void StopDB();
void WorldUpdateLoop();
void ClearOnlineAccounts();
void ShutdownCLIThread(std::thread* cliThread);
bool LoadRealmInfo();
variables_map GetConsoleArguments(int argc, char** argv, fs::path& configFile, std::string& cfg_service);

/// Launch the Trinity server
extern int main(int argc, char** argv)
{
    signal(SIGABRT, &Trinity::AbortHandler);

    auto configFile = fs::absolute(_TRINITY_CORE_CONFIG);
    std::string configService;

    auto vm = GetConsoleArguments(argc, argv, configFile, configService);
    // exit if help or version is enabled
    if (vm.count("help") || vm.count("version"))
        return 0;

    GOOGLE_PROTOBUF_VERIFY_VERSION;

#ifdef _WIN32
    if (configService.compare("install") == 0)
        return WinServiceInstall() ? 0 : 1;
    else if (configService.compare("uninstall") == 0)
        return WinServiceUninstall() ? 0 : 1;
    else if (configService.compare("run") == 0)
        return WinServiceRun() ? 0 : 0;
#endif

    std::string configError;
    if (!sConfigMgr->LoadInitial(configFile.generic_string(),
                                 std::vector<std::string>(argv, argv + argc),
                                 configError))
    {
        printf("Error in config file: %s\n", configError.c_str());
        return 1;
    }

    std::shared_ptr<Trinity::Asio::IoContext> ioContext = std::make_shared<Trinity::Asio::IoContext>();

    sLog->RegisterAppender<AppenderDB>();
    // If logs are supposed to be handled async then we need to pass the io_service into the Log singleton
    sLog->Initialize(sConfigMgr->GetBoolDefault("Log.Async.Enable", false) ? ioContext.get() : nullptr);

    TC_LOG_INFO("server.worldserver", "%s (worldserver-daemon)", GitRevision::GetFullVersion());
    TC_LOG_INFO("server.worldserver", "<Ctrl-C> to stop.\n");
    TC_LOG_INFO("server.worldserver", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    TC_LOG_INFO("server.worldserver", "xx Trinity 19/05/2016 - TrinityCore/tree/498725f3de8d18b26986c2c2e0ceec6c384a8016 xx");
	TC_LOG_INFO("server.worldserver", "xx                                                                                xx");
	TC_LOG_INFO("server.worldserver", "xx  Derniere maj du core 06/2022 - Stitch                                         xx");
    TC_LOG_INFO("server.worldserver", "xx                                                                                xx");
	TC_LOG_INFO("server.worldserver", "xx * Toutes les Classes pour toutes les Races                                     xx");
	TC_LOG_INFO("server.worldserver", "xx * Extension de classe Vampire pour le voleur                                   xx");
	TC_LOG_INFO("server.worldserver", "xx * Extension de classe posture Chaos pour le DK                                 xx");
	TC_LOG_INFO("server.worldserver", "xx * Rez sur le lieu de sa mort ou tp pdf                                         xx");
	TC_LOG_INFO("server.worldserver", "xx * Solocraft perso adapte les stats aux instances suivant le nbr de joueurs     xx");
	TC_LOG_INFO("server.worldserver", "xx * Pandaren : choix de la faction (quete & pnj change faction)                  xx");
	TC_LOG_INFO("server.worldserver", "xx * PNJ change race,faction,apparence,panda                      NPC 15000142    xx");
	TC_LOG_INFO("server.worldserver", "xx * Pet Demo utilisent desormais energie plutot que le mana                      xx");
	TC_LOG_INFO("server.worldserver", "xx * Pet & FakePlayers donnent de l'XP et sont plus efficace                      xx");
	TC_LOG_INFO("server.worldserver", "xx * Options de depart: lvl & lieu de depart dans sa faction      NPC 15000386    xx");
	TC_LOG_INFO("server.worldserver", "xx * Guilde par defaut a la creation d'un perso                                   xx");
	TC_LOG_INFO("server.worldserver", "xx * FunRate (joueur) stat,heal,dps,temps de cast,power...                        xx");
	TC_LOG_INFO("server.worldserver", "xx * DressNPC (creature_template_outfits , modelid1 a modelid4 en negatif)        xx");
	TC_LOG_INFO("server.worldserver", "xx * NPC_AI_xxx Script de classes et autres pour mobs classiques                  xx");
	TC_LOG_INFO("server.worldserver", "xx * Action suite a un evenement Joueur(Apprentissage, connexion, levelup, zone...xx");
	TC_LOG_INFO("server.worldserver", "xx                                                                                xx");
	TC_LOG_INFO("server.worldserver", "xx * Diverse majs & debugs                                                        xx");
	TC_LOG_INFO("server.worldserver", "xx * Un grand Merci a Noc & Scade pour leurs aides                                xx");
    TC_LOG_INFO("server.worldserver", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    TC_LOG_INFO("server.worldserver", " ");
    TC_LOG_INFO("server.worldserver", "Base sur le core Trinity 624 http://TrinityCore.org");
    TC_LOG_INFO("server.worldserver", "Using configuration file %s.", sConfigMgr->GetFilename().c_str());
    TC_LOG_INFO("server.worldserver", "Using SSL version: %s (library: %s)", OPENSSL_VERSION_TEXT, SSLeay_version(SSLEAY_VERSION));
    TC_LOG_INFO("server.worldserver", "Using Boost version: %i.%i.%i", BOOST_VERSION / 100000, BOOST_VERSION / 100 % 1000, BOOST_VERSION % 100);

    OpenSSLCrypto::threadsSetup(boost::dll::program_location().remove_filename());

    // Seed the OpenSSL's PRNG here.
    // That way it won't auto-seed when calling BigNumber::SetRand and slow down the first world login
    BigNumber seed;
    seed.SetRand(16 * 8);

    /// worldserver PID file creation
    std::string pidFile = sConfigMgr->GetStringDefault("PidFile", "");
    if (!pidFile.empty())
    {
        if (uint32 pid = CreatePIDFile(pidFile))
            TC_LOG_INFO("server.worldserver", "Daemon PID: %u\n", pid);
        else
        {
            TC_LOG_ERROR("server.worldserver", "Cannot create PID file %s.\n", pidFile.c_str());
            return 1;
        }
    }

    // Set signal handlers (this must be done before starting io_service threads, because otherwise they would unblock and exit)
    boost::asio::signal_set signals(*ioContext, SIGINT, SIGTERM);
#if PLATFORM == PLATFORM_WINDOWS
    signals.add(SIGBREAK);
#endif
    signals.async_wait(SignalHandler);

    // Start the Boost based thread pool
    int numThreads = sConfigMgr->GetIntDefault("ThreadPool", 1);
    std::shared_ptr<std::vector<std::thread>> threadPool(new std::vector<std::thread>(), [ioContext](std::vector<std::thread>* del)
    {
        ioContext->stop();
        for (std::thread& thr : *del)
            thr.join();

        delete del;
    });

    if (numThreads < 1)
        numThreads = 1;

    for (int i = 0; i < numThreads; ++i)
        threadPool->push_back(std::thread([ioContext]() { ioContext->run(); }));


    // Set process priority according to configuration settings
    SetProcessPriority("server.worldserver");

    // Start the databases
    if (!StartDB())
    {
        return 1;
    }

    // Set server offline (not connectable)
    LoginDatabase.DirectPExecute("UPDATE realmlist SET flag = flag | %u WHERE id = '%d'", REALM_FLAG_OFFLINE, realm.Id.Realm);

    sRealmList->Initialize(*ioContext, sConfigMgr->GetIntDefault("RealmsStateUpdateDelay", 10));

    LoadRealmInfo();

    sMetric->Initialize(realm.Name, *ioContext, []()
    {
        TC_METRIC_VALUE("online_players", sWorld->GetPlayerCount());
    });

    TC_METRIC_EVENT("events", "Worldserver started", "");

    // Initialize the World
    sScriptMgr->SetScriptLoader(AddScripts);
    sWorld->SetInitialWorldSettings();

    // Launch CliRunnable thread
    std::thread* cliThread = nullptr;
#ifdef _WIN32
//Stitch console active
   if (sConfigMgr->GetBoolDefault("Console.Enable", true) && (m_ServiceStatus == -1)/* need disable console in service mode*/)

#else
    if (sConfigMgr->GetBoolDefault("Console.Enable", true))
#endif
    {
        cliThread = new std::thread(CliThread);
    }

    // Start the Remote Access port (acceptor) if enabled
    AsyncAcceptor* raAcceptor = nullptr;
    if (sConfigMgr->GetBoolDefault("Ra.Enable", false))
        raAcceptor = StartRaSocketAcceptor(*ioContext);

    // Start soap serving thread if enabled
    std::thread* soapThread = nullptr;
    if (sConfigMgr->GetBoolDefault("SOAP.Enabled", false))
    {
        soapThread = new std::thread(TCSoapThread, sConfigMgr->GetStringDefault("SOAP.IP", "127.0.0.1"), uint16(sConfigMgr->GetIntDefault("SOAP.Port", 7878)));
    }

    // Launch the worldserver listener socket
    uint16 worldPort = uint16(sWorld->getIntConfig(CONFIG_PORT_WORLD));
    std::string worldListener = sConfigMgr->GetStringDefault("BindIP", "0.0.0.0");

    int networkThreads = sConfigMgr->GetIntDefault("Network.Threads", 1);

    if (networkThreads <= 0)
    {
        TC_LOG_ERROR("server.worldserver", "Network.Threads must be greater than 0");
        return false;
    }

    sWorldSocketMgr.StartNetwork(*ioContext, worldListener, worldPort, networkThreads);

    // Set server online (allow connecting now)
    LoginDatabase.DirectPExecute("UPDATE realmlist SET flag = flag & ~%u, population = 0 WHERE id = '%u'", REALM_FLAG_OFFLINE, realm.Id.Realm);
    realm.PopulationLevel = 0.0f;
    realm.Flags = RealmFlags(realm.Flags & ~uint32(REALM_FLAG_OFFLINE));

    TC_LOG_INFO("server.worldserver", "%s (worldserver-daemon) ready...", GitRevision::GetFullVersion());

    sScriptMgr->OnStartup();

    WorldUpdateLoop();

    // Shutdown starts here
    threadPool.reset();

    sLog->SetSynchronous();

    sScriptMgr->OnShutdown();

    sWorld->KickAll();                                       // save and kick all players
    sWorld->UpdateSessions(1);                             // real players unload required UpdateSessions call

    // unload battleground templates before different singletons destroyed
    sBattlegroundMgr->DeleteAllBattlegrounds();

    sWorldSocketMgr.StopNetwork();

    sInstanceSaveMgr->Unload();
    sOutdoorPvPMgr->Die();                    // unload it before MapManager
    sMapMgr->UnloadAll();                     // unload all grids (including locked in memory)
    sScriptMgr->Unload();
    sScriptReloadMgr->Unload();

    // set server offline
    LoginDatabase.DirectPExecute("UPDATE realmlist SET flag = flag | %u WHERE id = '%d'", REALM_FLAG_OFFLINE, realm.Id.Realm);
    sRealmList->Close();

    // Clean up threads if any
    if (soapThread != nullptr)
    {
        soapThread->join();
        delete soapThread;
    }

    delete raAcceptor;

    ///- Clean database before leaving
    ClearOnlineAccounts();

    StopDB();

    TC_METRIC_EVENT("events", "Worldserver shutdown", "");
    sMetric->ForceSend();

    TC_LOG_INFO("server.worldserver", "Halting process...");

    ShutdownCLIThread(cliThread);

    OpenSSLCrypto::threadsCleanup();

    google::protobuf::ShutdownProtobufLibrary();

    // 0 - normal shutdown
    // 1 - shutdown at error
    // 2 - restart command used, this code can be used by restarter for restart Trinityd

    return World::GetExitCode();
}

void ShutdownCLIThread(std::thread* cliThread)
{
    if (cliThread != nullptr)
    {
#ifdef _WIN32
        // First try to cancel any I/O in the CLI thread
        if (!CancelSynchronousIo(cliThread->native_handle()))
        {
            // if CancelSynchronousIo() fails, print the error and try with old way
            DWORD errorCode = GetLastError();
            LPCSTR errorBuffer;

            DWORD formatReturnCode = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
                                                   nullptr, errorCode, 0, (LPTSTR)&errorBuffer, 0, nullptr);
            if (!formatReturnCode)
                errorBuffer = "Unknown error";

            TC_LOG_DEBUG("server.worldserver", "Error cancelling I/O of CliThread, error code %u, detail: %s",
                uint32(errorCode), errorBuffer);
            LocalFree((LPSTR)errorBuffer);

            // send keyboard input to safely unblock the CLI thread
            INPUT_RECORD b[4];
            HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
            b[0].EventType = KEY_EVENT;
            b[0].Event.KeyEvent.bKeyDown = TRUE;
            b[0].Event.KeyEvent.uChar.AsciiChar = 'X';
            b[0].Event.KeyEvent.wVirtualKeyCode = 'X';
            b[0].Event.KeyEvent.wRepeatCount = 1;

            b[1].EventType = KEY_EVENT;
            b[1].Event.KeyEvent.bKeyDown = FALSE;
            b[1].Event.KeyEvent.uChar.AsciiChar = 'X';
            b[1].Event.KeyEvent.wVirtualKeyCode = 'X';
            b[1].Event.KeyEvent.wRepeatCount = 1;

            b[2].EventType = KEY_EVENT;
            b[2].Event.KeyEvent.bKeyDown = TRUE;
            b[2].Event.KeyEvent.dwControlKeyState = 0;
            b[2].Event.KeyEvent.uChar.AsciiChar = '\r';
            b[2].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
            b[2].Event.KeyEvent.wRepeatCount = 1;
            b[2].Event.KeyEvent.wVirtualScanCode = 0x1c;

            b[3].EventType = KEY_EVENT;
            b[3].Event.KeyEvent.bKeyDown = FALSE;
            b[3].Event.KeyEvent.dwControlKeyState = 0;
            b[3].Event.KeyEvent.uChar.AsciiChar = '\r';
            b[3].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
            b[3].Event.KeyEvent.wVirtualScanCode = 0x1c;
            b[3].Event.KeyEvent.wRepeatCount = 1;
            DWORD numb;
            WriteConsoleInput(hStdIn, b, 4, &numb);
        }
#endif
        cliThread->join();
        delete cliThread;
    }
}

void WorldUpdateLoop()
{
    uint32 realCurrTime = 0;
    uint32 realPrevTime = getMSTime();

    uint32 prevSleepTime = 0;                               // used for balanced full tick time length near WORLD_SLEEP_CONST

    ///- While we have not World::m_stopEvent, update the world
    while (!World::IsStopped())
    {
        ++World::m_worldLoopCounter;
        realCurrTime = getMSTime();

        uint32 diff = getMSTimeDiff(realPrevTime, realCurrTime);

        sWorld->Update(diff);
        realPrevTime = realCurrTime;

        // diff (D0) include time of previous sleep (d0) + tick time (t0)
        // we want that next d1 + t1 == WORLD_SLEEP_CONST
        // we can't know next t1 and then can use (t0 + d1) == WORLD_SLEEP_CONST requirement
        // d1 = WORLD_SLEEP_CONST - t0 = WORLD_SLEEP_CONST - (D0 - d0) = WORLD_SLEEP_CONST + d0 - D0
        if (diff <= WORLD_SLEEP_CONST + prevSleepTime)
        {
            prevSleepTime = WORLD_SLEEP_CONST + prevSleepTime - diff;

            std::this_thread::sleep_for(std::chrono::milliseconds(prevSleepTime));
        }
        else
            prevSleepTime = 0;

#ifdef _WIN32
        if (m_ServiceStatus == 0)
            World::StopNow(SHUTDOWN_EXIT_CODE);

        while (m_ServiceStatus == 2)
            Sleep(1000);
#endif
    }
}

void SignalHandler(const boost::system::error_code& error, int /*signalNumber*/)
{
    if (!error)
        World::StopNow(SHUTDOWN_EXIT_CODE);
}

AsyncAcceptor* StartRaSocketAcceptor(Trinity::Asio::IoContext& ioContext)
{
    uint16 raPort = uint16(sConfigMgr->GetIntDefault("Ra.Port", 3443));
    std::string raListener = sConfigMgr->GetStringDefault("Ra.IP", "0.0.0.0");

    AsyncAcceptor* acceptor = new AsyncAcceptor(ioContext, raListener, raPort);
    acceptor->AsyncAccept<RASession>();
    return acceptor;
}

bool LoadRealmInfo()
{
    boost::asio::ip::tcp::resolver resolver(ioContext);
    boost::asio::ip::tcp::resolver::iterator end;

    TC_LOG_INFO("server.worldserver", "Loading realm info for realm ID %u", realm.Id.Realm);

    QueryResult result = LoginDatabase.PQuery("SELECT id, name, address, localAddress, localSubnetMask, port, icon, flag, timezone, allowedSecurityLevel, population, gamebuild, Region, Battlegroup FROM realmlist WHERE id = %u", realm.Id.Realm);
    if (!result)
    {
        TC_LOG_ERROR("server.worldserver", "Failed to query realm info for realm ID %u", realm.Id.Realm);
        return false;
    }

    Field* fields = result->Fetch();
    if (!fields)
    {
        TC_LOG_ERROR("server.worldserver", "Failed to fetch fields for realm ID %u", realm.Id.Realm);
        return false;
    }

    try
    {
        realm.Name = fields[1].GetString();
        TC_LOG_INFO("server.worldserver", "Realm name: %s", realm.Name.c_str());

        std::string externalAddress = fields[2].GetString();
        TC_LOG_INFO("server.worldserver", "External address: %s", externalAddress.c_str());

        boost::asio::ip::tcp::resolver::query externalAddressQuery(boost::asio::ip::tcp::v4(), externalAddress, "");
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver::iterator endPoint = resolver.resolve(externalAddressQuery, ec);
        if (endPoint == end || ec)
        {
            TC_LOG_ERROR("server.worldserver", "Could not resolve external address %s", externalAddress.c_str());
            return false;
        }

        realm.ExternalAddress = (*endPoint).endpoint().address();
        TC_LOG_INFO("server.worldserver", "Resolved external address: %s", realm.ExternalAddress.to_string().c_str());

        std::string localAddress = fields[3].GetString();
        TC_LOG_INFO("server.worldserver", "Local address: %s", localAddress.c_str());

        boost::asio::ip::tcp::resolver::query localAddressQuery(boost::asio::ip::tcp::v4(), localAddress, "");
        endPoint = resolver.resolve(localAddressQuery, ec);
        if (endPoint == end || ec)
        {
            TC_LOG_ERROR("server.worldserver", "Could not resolve local address %s", localAddress.c_str());
            return false;
        }

        realm.LocalAddress = (*endPoint).endpoint().address();
        TC_LOG_INFO("server.worldserver", "Resolved local address: %s", realm.LocalAddress.to_string().c_str());
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("server.worldserver", "Exception caught while loading realm info: %s", e.what());
        return false;
    }

    return true;
}

/// Initialize connection to the databases
bool StartDB()
{
    MySQL::Library_Init();

    // Load databases
    DatabaseLoader loader("server.worldserver", DatabaseLoader::DATABASE_NONE);
    loader
        .AddDatabase(LoginDatabase, "Login")
        .AddDatabase(CharacterDatabase, "Character")
        .AddDatabase(WorldDatabase, "World")
        .AddDatabase(HotfixDatabase, "Hotfix");

    if (!loader.Load())
        return false;

    ///- Get the realm Id from the configuration file
    realm.Id.Realm = sConfigMgr->GetIntDefault("RealmID", 0);
    if (!realm.Id.Realm)
    {
        TC_LOG_ERROR("server.worldserver", "Realm ID not defined in configuration file");
        return false;
    }

    TC_LOG_INFO("server.worldserver", "Realm running as realm ID %u", realm.Id.Realm);

    ///- Clean the database before starting
    ClearOnlineAccounts();

    ///- Insert version info into DB
    WorldDatabase.PExecute("UPDATE version SET core_version = '%s', core_revision = '%s'", GitRevision::GetFullVersion(), GitRevision::GetHash());        // One-time query

    sWorld->LoadDBVersion();

    TC_LOG_INFO("server.worldserver", "Using World DB: %s", sWorld->GetDBVersion());
    return true;
}

void StopDB()
{
    CharacterDatabase.Close();
    WorldDatabase.Close();
    LoginDatabase.Close();

    MySQL::Library_End();
}

/// Clear 'online' status for all accounts with characters in this realm
void ClearOnlineAccounts()
{
    // Reset online status for all accounts with characters on the current realm
    LoginDatabase.DirectPExecute("UPDATE account SET online = 0 WHERE online > 0 AND id IN (SELECT acctid FROM realmcharacters WHERE realmid = %d)", realm.Id.Realm);

    // Reset online status for all characters
    CharacterDatabase.DirectExecute("UPDATE characters SET online = 0 WHERE online <> 0");

    // Battleground instance ids reset at server restart
    CharacterDatabase.DirectExecute("UPDATE character_battleground_data SET instanceId = 0");
}

/// @}

variables_map GetConsoleArguments(int argc, char** argv, fs::path& configFile, std::string& configService)
{
    // Silences warning about configService not be used if the OS is not Windows
    (void)configService;

    options_description all("Allowed options");
    all.add_options()
        ("help,h", "print usage message")
        ("version,v", "print version build info")
        ("config,c", value<fs::path>(&configFile)->default_value(fs::absolute(_TRINITY_CORE_CONFIG)),
                     "use <arg> as configuration file")
        ;
#ifdef _WIN32
    options_description win("Windows platform specific options");
    win.add_options()
        ("service,s", value<std::string>(&configService)->default_value(""), "Windows service options: [install | uninstall]")
        ;

    all.add(win);
#endif
    variables_map vm;
    try
    {
        store(command_line_parser(argc, argv).options(all).allow_unregistered().run(), vm);
        notify(vm);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    if (vm.count("help"))
    {
        std::cout << all << "\n";
    }
    else if (vm.count("version"))
    {
        std::cout << GitRevision::GetFullVersion() << "\n";
    }

    return vm;
}