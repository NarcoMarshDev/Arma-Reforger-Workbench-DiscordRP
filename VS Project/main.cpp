#define _CRT_SECURE_NO_WARNINGS
//#define CONSOLEAPP

#include <array>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <Windows.h>
#include "discord-files/discord.h"

struct DiscordState {
    discord::User currentUser;

    std::unique_ptr<discord::Core> core;
};

namespace {
    volatile bool interrupted{ false };
}

std::wstring ExePath() {
    TCHAR buffer[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
    return std::wstring(buffer).substr(0, pos);
}
//int WinMain(int, char**)
int wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    DiscordState state{};

    discord::Core* core{};
    auto result = discord::Core::Create(1050436054630551683, DiscordCreateFlags_Default, &core);
    state.core.reset(core);
    if (!state.core) {
        std::cout << "Failed to instantiate discord core! (err " << static_cast<int>(result)
            << ")\n";
        std::exit(-1);
    }
    
    char currentFile[50] = "No file open...";
    char currentAddon[50] = "Arma Reforger";
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

        //auto windowHandle = WIN32::GetForegroundWindow();
        //auto id = WIN32::GetWindowThreadProcessId(windowHandle, NULL);
        //std::cout << id << std::endl;

        std::wstring path = ExePath();
        std::wstring filename = L"\\RPState.txt";
        path += filename;
        char cstr[256];
        wcstombs(cstr, path.c_str(), 256);

        //std::wcout << cstr << std::endl;
        //std::cout << currentFile << std::endl;
        //std::cout << currentAddon << std::endl;
        //std::cout << currentModule << std::endl;
        //std::cout << "----------------" << std::endl;

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