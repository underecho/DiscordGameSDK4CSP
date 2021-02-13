// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "pch.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <windows.h>
#include "discord-files/discord.h"
#include "clipstudio-sdk/TriglavPlugInSDK.h"


WNDPROC oriWndProc = NULL;
HWND hWnd = ::FindWindowEx(0, 0, L"742DEA58-ED6B-4402-BC11-20DFC6D08040", 0);

struct Application {
    struct IDiscordCore* core;
    struct IDiscordUsers* users;
};

struct DiscordState {
	std::unique_ptr<discord::Core> core;
};

namespace {
	volatile bool interrupted{ false };
};

struct	DiscordPluginInfo
{
	TriglavPlugInPropertyService* pPropertyService;
};

discord::Activity activity{};
DiscordState state{};
int timestump;

LRESULT CALLBACK hWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_ACTIVATE)
	{
		if (wParam == 1 || wParam == 2){
			activity.SetDetails("Drawing");
			activity.GetAssets().SetSmallImage("drawing");
			activity.GetAssets().SetSmallText("Drawing");
			MessageBox(nullptr, ((LPCWSTR) wParam), L"test", MB_OK);
		}
		else {
			activity.SetDetails("Inactive");
			activity.SetState("Pop Snacks");
			activity.GetAssets().SetSmallImage("inactive");
			activity.GetAssets().SetSmallText("Inactive");
		}
		MessageBox(nullptr, ((LPCWSTR)wParam), L"test", MB_OK);
		state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
			std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
				<< " updating activity!\n";
			});
		return 1L;
	}

	return CallWindowProc(oriWndProc, hwnd, uMsg, wParam, lParam);

}

DWORD WINAPI Creation(LPVOID)
{
	oriWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG)(LONG_PTR)hWndProc);
	// MessageBox(nullptr, L"testr", L"test", MB_OK);
	return TRUE;
}

void __stdcall discordEntry() {
	DiscordState state{};
	discord::Core* core{};
	auto response = discord::Core::Create(771084728300339251, DiscordCreateFlags_Default, &core);
	state.core.reset(core);

	if (!state.core) {
		std::cout << "Failed to instantiate Discord!";
		std::exit(-1);
	}

	discord::Activity activity{};
	activity.SetDetails("Drawing");
	activity.SetState("Powered by RPC4CSP");
	activity.GetAssets().SetSmallImage("drawing");
	activity.GetAssets().SetSmallText("Drawing");
	activity.GetAssets().SetLargeImage("clipstudio");
	activity.GetAssets().SetLargeText("Clip Studio Paint");
	activity.GetTimestamps().SetStart(time(NULL));
	activity.SetType(discord::ActivityType::Streaming);
	state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
		std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
			<< " updating activity!\n";
		});

	do {
		state.core->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	} while (!interrupted);
	
};

VOID CALLBACK WinEventProcCallback(HWINEVENTHOOK hWinEventHook, DWORD dwEvent, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
	if (dwEvent == EVENT_SYSTEM_FOREGROUND)
	{
		 MessageBox(NULL, L"aa", L"test", MB_OK);
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	static std::thread* thread;
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);

		thread = new std::thread(discordEntry);
		// oriWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG)(LONG_PTR)hWndProc);
		
	}
	else if(ul_reason_for_call == DLL_PROCESS_DETACH){
		interrupted = true;
		delete thread;
	}
    return TRUE;
}

/*__declspec(dllexport) void TRIGLAV_PLUGIN_API TriglavPluginCall(TriglavPlugInInt* result, TriglavPlugInPtr* data, TriglavPlugInInt
	selector, TriglavPlugInServer* pluginServer, TriglavPlugInPtr reserved) {

}
*/

void TRIGLAV_PLUGIN_API TriglavPluginCall(TriglavPlugInInt* result, TriglavPlugInPtr* data, TriglavPlugInInt selector, TriglavPlugInServer* pluginServer, TriglavPlugInPtr reserved)
{
	*result = kTriglavPlugInCallResultFailed;
	if (pluginServer != NULL)
	{
		if (selector == kTriglavPlugInSelectorModuleInitialize)
		{
			//	プラグインの初期化
			TriglavPlugInModuleInitializeRecord* pModuleInitializeRecord = (*pluginServer).recordSuite.moduleInitializeRecord;
			TriglavPlugInStringService* pStringService = (*pluginServer).serviceSuite.stringService;
			if (pModuleInitializeRecord != NULL && pStringService != NULL)
			{
				TriglavPlugInInt	hostVersion;
				(*pModuleInitializeRecord).getHostVersionProc(&hostVersion, (*pluginServer).hostObject);
				if (hostVersion >= kTriglavPlugInNeedHostVersion)
				{
					TriglavPlugInStringObject	moduleID = NULL;
					const char* moduleIDString = "1B8C95E4-4DDA-45D5-9F35-08560367B22F"; // random GUID
					(*pStringService).createWithAsciiStringProc(&moduleID, moduleIDString, static_cast<TriglavPlugInInt>(::strlen(moduleIDString)));
					(*pModuleInitializeRecord).setModuleIDProc((*pluginServer).hostObject, moduleID);
					(*pModuleInitializeRecord).setModuleKindProc((*pluginServer).hostObject, kTriglavPlugInModuleSwitchKindFilter);
					(*pStringService).releaseProc(moduleID); // release module ID

					DiscordPluginInfo* pFilterInfo = new DiscordPluginInfo;
					*data = pFilterInfo;
					*result = kTriglavPlugInCallResultSuccess;
				}
			}
		}
		else if (selector == kTriglavPlugInSelectorModuleTerminate)
		{
			//	プラグインの終了処理
			DiscordPluginInfo* pFilterInfo = static_cast<DiscordPluginInfo*>(*data);
			delete pFilterInfo;
			*data = NULL;
			*result = kTriglavPlugInCallResultSuccess;
		}
	}
	return;
}