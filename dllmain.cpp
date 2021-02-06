// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include "discord-files/discord.h"

struct Application {
    struct IDiscordCore* core;
    struct IDiscordUsers* users;
};

struct DiscordState {
	std::unique_ptr<discord::Core> core;
};

namespace {
	volatile bool interrupted{ false };
}

void __stdcall discordEntry() {
	MessageBox(nullptr, L"idiot", L"", MB_OKCANCEL);
	DiscordState state{};
	discord::Core* core{};
	MessageBox(nullptr, L"baka", L"", MB_OKCANCEL);
	auto response = discord::Core::Create(461618159171141643, DiscordCreateFlags_Default, &core);
	state.core.reset(core);

	if (!state.core) {
		std::cout << "Failed to instantiate Discord!";
		std::exit(-1);
	}

	discord::Activity activity{};
	activity.SetDetails("Fruit Tarts");
	activity.SetState("Pop Snacks");
	activity.GetAssets().SetSmallImage("the");
	activity.GetAssets().SetSmallText("i mage");
	activity.GetAssets().SetLargeImage("the");
	activity.GetAssets().SetLargeText("u mage");
	activity.SetType(discord::ActivityType::Playing);
	state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
		std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
			<< " updating activity!\n";
		});
	std::signal(SIGINT, [](int) {
		interrupted = true;
		});
	MessageBox(nullptr, L"a", L"", MB_OKCANCEL);
	do {
		state.core->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	} while (!interrupted);
	// FreeLibraryAndExitThread(static_cast<HMODULE> (lpParameter), EXIT_SUCCESS);
};

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		MessageBox(nullptr, L"b", L"", MB_OKCANCEL);
		DisableThreadLibraryCalls(hModule);
		
		// auto hThread = std::thread(discordEntry);
		// ::WaitForSingleObject(hThread, INFINITE);

		// DWORD dwExitCode = hThread.join();

		discordEntry();

		// hThread.join();

		// std::wcout << L"スレッド終了コード( " << dwExitCode << L" )" << std::endl;

	}
    return TRUE;
}

