#include "pch.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <codecvt>
#include <windows.h>
#include <comdef.h>
#include "discord-files/discord.h"
#include "clipstudio-sdk/TriglavPlugInSDK.h"

#define BASEADDRESS 0x0386A6F0
#define OFFSET1 0xC8
#define OFFSET2 0x188
#define OFFSET3 0x200

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

time_t time(time_t* timer);
time_t timestamp_start;
time_t timestamp_delta;
auto base_address = reinterpret_cast<uint64_t>(GetModuleHandleA(nullptr));

static auto get_value_from_multi_pointer(const uint64_t base_address) {
	uint64_t address = base_address;
	auto handle = GetCurrentProcess();
	uint64_t title_base = NULL;
	auto pTitle = reinterpret_cast<uintptr_t*>(address + BASEADDRESS);
	title_base = *pTitle;

	// ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address + BASEADDRESS), &title_base, sizeof(uint64_t), nullptr);
	uint64_t first_dereference = NULL;
	ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(title_base + OFFSET1), &first_dereference, sizeof(uint64_t), nullptr);
	_com_error err(GetLastError());
	std::cout << err.ErrorMessage() << std::endl;
	uint64_t second_dereference = NULL;
	ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(first_dereference + OFFSET2), &second_dereference, sizeof(uint64_t), nullptr);
	uint64_t third_dereference = NULL;
	ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(second_dereference + OFFSET3), &third_dereference, sizeof(uint64_t), nullptr);

	return third_dereference;
}

// Discord RPC Entry
void __stdcall discordEntry() {
	auto response = discord::Core::Create(771084728300339251, DiscordCreateFlags_Default, &core);
	state.core.reset(core);

	if (!state.core) {
		std::cout << "Failed to instantiate Discord!";
		std::exit(-1);
	}
	time(&timestamp_start);
	activity.SetDetails("Drawing");
	activity.SetState("Powered by RPC4CSP");
	activity.GetAssets().SetSmallImage("drawing");
	activity.GetAssets().SetSmallText("Drawing");
	activity.GetAssets().SetLargeImage("clipstudio");
	activity.GetAssets().SetLargeText("Clip Studio Paint");
	activity.GetTimestamps().SetStart(timestamp_start);
	activity.SetType(discord::ActivityType::Watching);
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

	switch (uMsg) {

	case WM_ACTIVATEAPP:
		if (wParam == TRUE) {
			uint64_t text;
			text = get_value_from_multi_pointer(base_address);
			LPCWSTR lpcwTitleText = (LPCWSTR) text;
			timestamp_start = time(NULL) - timestamp_delta;
			activity.GetTimestamps().SetStart(timestamp_start);

			std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
			std::string titleText = cvt.to_bytes(lpcwTitleText); // c++ の std::string
			std::string filenameText = "";
			filenameText = titleText.substr(0, titleText.find_last_of('('));

			auto rawFilenameText = filenameText.c_str(); // char *

			activity.SetDetails(rawFilenameText); // wchar_t → utf8
			activity.GetAssets().SetSmallImage("drawing");
			activity.GetAssets().SetSmallText("Drawing");
		}
		else { // Window going to background
			timestamp_delta = time(NULL) - timestamp_start;
			activity.GetTimestamps().SetStart(0);
			activity.SetDetails("Inactive");
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
		thread = new std::thread(discordEntry); // make Discord RPC thread
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


