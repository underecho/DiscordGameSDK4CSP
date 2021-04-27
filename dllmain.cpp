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

struct Application {
    struct IDiscordCore* core;
    struct IDiscordUsers* users;
};

typedef struct {
	_In_ DWORD dwProcessId;
	_Out_ HWND hWnd;
} HANDLE_DATA;


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

WNDPROC originalProc;
discord::Activity activity{};
DiscordState state{};
discord::Core* core{};
int timestamp;
char status;

// Discord RPC Entry

void __stdcall discordEntry() {
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

// Window handle helper
static FORCEINLINE BOOL IsMainWindow(HWND hWnd)
{
	return (!GetWindow(hWnd, GW_OWNER)) && IsWindowVisible(hWnd);
}

static BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam)
{
	HANDLE_DATA* hdCallbackData;
	DWORD dwProcessId;
	hdCallbackData = (HANDLE_DATA*)lParam;
	(VOID)GetWindowThreadProcessId(hWnd, &dwProcessId);
	if (hdCallbackData->dwProcessId != dwProcessId || !IsMainWindow(hWnd)) {
		return TRUE;
	}
	hdCallbackData->hWnd = hWnd;
	return FALSE;
}

HWND APIENTRY GetMainWindow(DWORD dwProcessId)
{
	HANDLE_DATA hdCallbackData;
	hdCallbackData.dwProcessId = dwProcessId;
	hdCallbackData.hWnd = NULL;
	(VOID)EnumWindows(EnumWindowsCallback, (LPARAM)&hdCallbackData);
	return hdCallbackData.hWnd;
}

// Override WindowProc
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HHOOK hhk = NULL;

	switch (uMsg) {

	case WM_ACTIVATEAPP:
		if (wParam == TRUE) {
			activity.SetDetails("Drawing");
			activity.GetAssets().SetSmallImage("drawing");
			activity.GetAssets().SetSmallText("Drawing");
		}
		else {
			activity.SetDetails("Inactive");
			// activity.SetState("Pop Snacks");
			activity.GetAssets().SetSmallImage("inactive");
			activity.GetAssets().SetSmallText("Inactive");
		}
		state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
			std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
				<< " updating activity!\n";
			});
		break;

	default:
		break;
	}
	return CallWindowProc(originalProc, hwnd, uMsg, wParam, lParam);
}

// DLL EntryPoint

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	static std::thread* thread;
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
		originalProc = (WNDPROC)SetWindowLongPtr(GetMainWindow(GetCurrentProcessId()), GWLP_WNDPROC, (LONG_PTR)WindowProc);
		thread = new std::thread(discordEntry);
		
	}
	else if(ul_reason_for_call == DLL_PROCESS_DETACH){
		interrupted = true;
		delete thread;
	}
    return TRUE;
}

// CSP Plugin Entry
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


