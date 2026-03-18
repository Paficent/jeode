#include "proxy/proxy.h"

extern "C" {
FARPROC orig_WinHttpOpen = nullptr;
FARPROC orig_WinHttpConnect = nullptr;
FARPROC orig_WinHttpSendRequest = nullptr;
FARPROC orig_WinHttpCloseHandle = nullptr;
FARPROC orig_WinHttpOpenRequest = nullptr;
FARPROC orig_WinHttpReadData = nullptr;
FARPROC orig_WinHttpQueryHeaders = nullptr;
FARPROC orig_WinHttpAddRequestHeaders = nullptr;
FARPROC orig_WinHttpQueryDataAvailable = nullptr;
FARPROC orig_WinHttpReceiveResponse = nullptr;
}

bool proxy_init_winhttp() {
	wchar_t sysDir[MAX_PATH];
	wchar_t dllPath[MAX_PATH];
	GetSystemDirectoryW(sysDir, MAX_PATH);
	wsprintfW(dllPath, L"%s\\winhttp.dll", sysDir);

	HMODULE hReal = LoadLibraryW(dllPath);
	if (!hReal) return false;

#define RESOLVE(name) orig_##name = GetProcAddress(hReal, #name)
	RESOLVE(WinHttpOpen);
	RESOLVE(WinHttpConnect);
	RESOLVE(WinHttpSendRequest);
	RESOLVE(WinHttpCloseHandle);
	RESOLVE(WinHttpOpenRequest);
	RESOLVE(WinHttpReadData);
	RESOLVE(WinHttpQueryHeaders);
	RESOLVE(WinHttpAddRequestHeaders);
	RESOLVE(WinHttpQueryDataAvailable);
	RESOLVE(WinHttpReceiveResponse);
#undef RESOLVE

	return true;
}
