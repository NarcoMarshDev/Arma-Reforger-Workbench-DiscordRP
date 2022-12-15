#define _CRT_SECURE_NO_WARNINGS
//#define WORKBENCH_RP_DEBUG

#include <array>
#include <map>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <Windows.h>
#include <TlHelp32.h>
#include "discord-files/discord.h"

struct DiscordState {
    discord::User currentUser;

    std::unique_ptr<discord::Core> core;
};

namespace {
    volatile bool interrupted{ false };
}

// find process ID by process name
DWORD GetProcessPID(const wchar_t* procname) {

    HANDLE hSnapshot;
    PROCESSENTRY32 pe;
    DWORD pid = 0;
    BOOL hResult;

    // snapshot of all processes in the system
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hSnapshot) return 0;

    // initializing size: needed for using Process32First
    pe.dwSize = sizeof(PROCESSENTRY32);

    // info about first process encountered in a system snapshot
    hResult = Process32First(hSnapshot, &pe);

    // retrieve information about the processes
    // and exit if unsuccessful
    while (hResult) {
        // if we find the process: return process ID
        if (wcscmp(procname, pe.szExeFile) == 0) {
            pid = pe.th32ProcessID;
            break;
        }
        hResult = Process32Next(hSnapshot, &pe);
    }

    // closes an open handle (CreateToolhelp32Snapshot)
    CloseHandle(hSnapshot);
    return pid;
}

// Get the path to the executable at runtime
std::wstring ExePath() {
    TCHAR buffer[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
    return std::wstring(buffer).substr(0, pos);
}

// Map of different workbench versions' executables and related discord app id's (todo - create discord apps for other versions)
std::map<const wchar_t*, int64_t> workbenchVersions = { 
{L"ArmaReforgerWorkbenchSteam.exe"  , 1050436054630551683}, // Reforger steam
{L"ArmaReforgerWorkbench.exe"       , 1050436054630551683}, // Reforger non-steam
{L""                                , 1050436054630551683}, // Arma 4 steam
{L""                                , 1050436054630551683}, // Arma 4 non-steam
{L"workbenchApp.exe"                , 1050436054630551683}  // DayZ
};

//int main()
//int WinMain(int, char**)
int wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    // determine workbench version being run
    int64_t discordAppID = 0;
    wchar_t workbenchEXE[64];
    for (const auto& pair : workbenchVersions)
    {
        DWORD pid = GetProcessPID(pair.first);
        if (pid != 0) {
            memcpy(workbenchEXE, pair.first, 64);
            discordAppID = pair.second;
            break;
        }
    }
    // if no workbench is found something has gone wrong, exit the program
    if (discordAppID == 0) {
        MessageBoxW(NULL, L"No instance of enfusion workbench found, cannot start!", L"Enfusion Workbench Rich Presence", MB_ICONERROR | MB_OK);
        return 1;
    }

    DiscordState state{};

    discord::Core* core{};
    auto result = discord::Core::Create(discordAppID, DiscordCreateFlags_Default, &core);
    state.core.reset(core);
    if (!state.core) {
        std::cout << "Failed to instantiate discord core! (err " << static_cast<int>(result)
            << ")\n";
        std::exit(-1);
    }
    
    char currentFile[50] = "No file open...";
    char currentAddon[50] = "";
    char currentModule[50] = "";

    discord::Activity activity{};

    activity.SetDetails(currentFile);
    activity.SetState(currentAddon);
    activity.GetAssets().SetLargeImage("logobg");
    activity.GetAssets().SetLargeText("Developing...");
    activity.SetType(discord::ActivityType::Playing);
    activity.GetTimestamps().SetStart(std::time(0));
    state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {nullptr;});

    std::signal(SIGINT, [](int) { interrupted = true; });
    bool update = true;
    do {
        state.core->RunCallbacks();

        // check if workbench is still open, if not exit
        if (!GetProcessPID(workbenchEXE)) {
            return 0;
        }

        std::wstring path = ExePath();
        std::wstring filename = L"\\RPState.txt";
        path += filename;
        char cstr[256];
        wcstombs(cstr, path.c_str(), 256);

#ifdef WORKBENCH_RP_DEBUG
        std::wcout << cstr << std::endl;
        std::cout << currentFile << std::endl;
        std::cout << currentAddon << std::endl;
        std::cout << currentModule << std::endl;
        std::cout << "----------------" << std::endl;
#endif
        // open file
        FILE* file = std::fopen(cstr, "r");
        if (!file) {
            std::cout << "No File" << std::endl;
        }
        if (file) {

            char line[100];
            // get current file name
            fgets(line, 100, file);
            //std::cout << line;
            if (line != currentFile) {
                activity.SetDetails(line);
                memcpy(currentFile, line, 50);
                update = true;
            }
            // get main addon name
            fgets(line, 100, file);
            //std::cout << line;
            if (line != currentAddon) {
                activity.SetState(line);
                memcpy(currentAddon, line, 50);
                update = true;
            }
            // get current module name
            fgets(line, 100, file);
            //std::cout << line << "\n" << std::endl;
            if (line != currentModule) {
                activity.GetAssets().SetSmallImage(strtok(line, "\n")); // use one call of strtok() to get the first part of the string before the \n
                memcpy(currentAddon, line, 50);
                update = true;
            }

            std::fclose(file);

            if (update) {
                state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {nullptr; });
                update = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    } while (!interrupted);

    return 0;
}