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
FARPROC orig_WinHttpAddRequestHeadersEx = nullptr;
FARPROC orig_WinHttpCheckPlatform = nullptr;
FARPROC orig_WinHttpCrackUrl = nullptr;
FARPROC orig_WinHttpCreateProxyResolver = nullptr;
FARPROC orig_WinHttpCreateUrl = nullptr;
FARPROC orig_WinHttpDetectAutoProxyConfigUrl = nullptr;
FARPROC orig_WinHttpFreeProxyResult = nullptr;
FARPROC orig_WinHttpFreeProxySettingsEx = nullptr;
FARPROC orig_WinHttpFreeQueryConnectionGroupResult = nullptr;
FARPROC orig_WinHttpGetDefaultProxyConfiguration = nullptr;
FARPROC orig_WinHTTPGetIEProxyConfigForCurrentUser = nullptr;
FARPROC orig_WinHttpGetProxyForUrl = nullptr;
FARPROC orig_WinHttpGetProxyForUrlEx = nullptr;
FARPROC orig_WinHttpGetProxyResult = nullptr;
FARPROC orig_WinHttpGetProxySettingsEx = nullptr;
FARPROC orig_WinHttpGetProxySettingsResultEx = nullptr;
FARPROC orig_WinHttpQueryAuthSchemes = nullptr;
FARPROC orig_WinHttpQueryConnectionGroup = nullptr;
FARPROC orig_WinHttpQueryHeadersEx = nullptr;
FARPROC orig_WinHttpQueryOption = nullptr;
FARPROC orig_WinHttpReadDataEx = nullptr;
FARPROC orig_WinHttpRegisterProxyChangeNotification = nullptr;
FARPROC orig_WinHttpResetAutoProxy = nullptr;
FARPROC orig_WinHttpSetCredentials = nullptr;
FARPROC orig_WinHttpSetDefaultProxyConfiguration = nullptr;
FARPROC orig_WinHttpSetOption = nullptr;
FARPROC orig_WinHttpSetStatusCallback = nullptr;
FARPROC orig_WinHttpSetTimeouts = nullptr;
FARPROC orig_WinHttpTimeFromSystemTime = nullptr;
FARPROC orig_WinHttpTimeToSystemTime = nullptr;
FARPROC orig_WinHttpUnregisterProxyChangeNotification = nullptr;
FARPROC orig_WinHttpWriteData = nullptr;
FARPROC orig_WinHttpWebSocketClose = nullptr;
FARPROC orig_WinHttpWebSocketCompleteUpgrade = nullptr;
FARPROC orig_WinHttpWebSocketQueryCloseStatus = nullptr;
FARPROC orig_WinHttpWebSocketReceive = nullptr;
FARPROC orig_WinHttpWebSocketSend = nullptr;
FARPROC orig_WinHttpWebSocketShutdown = nullptr;
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
	RESOLVE(WinHttpAddRequestHeadersEx);
	RESOLVE(WinHttpCheckPlatform);
	RESOLVE(WinHttpCrackUrl);
	RESOLVE(WinHttpCreateProxyResolver);
	RESOLVE(WinHttpCreateUrl);
	RESOLVE(WinHttpDetectAutoProxyConfigUrl);
	RESOLVE(WinHttpFreeProxyResult);
	RESOLVE(WinHttpFreeProxySettingsEx);
	RESOLVE(WinHttpFreeQueryConnectionGroupResult);
	RESOLVE(WinHttpGetDefaultProxyConfiguration);
	RESOLVE(WinHTTPGetIEProxyConfigForCurrentUser);
	RESOLVE(WinHttpGetProxyForUrl);
	RESOLVE(WinHttpGetProxyForUrlEx);
	RESOLVE(WinHttpGetProxyResult);
	RESOLVE(WinHttpGetProxySettingsEx);
	RESOLVE(WinHttpGetProxySettingsResultEx);
	RESOLVE(WinHttpQueryAuthSchemes);
	RESOLVE(WinHttpQueryConnectionGroup);
	RESOLVE(WinHttpQueryHeadersEx);
	RESOLVE(WinHttpQueryOption);
	RESOLVE(WinHttpReadDataEx);
	RESOLVE(WinHttpRegisterProxyChangeNotification);
	RESOLVE(WinHttpResetAutoProxy);
	RESOLVE(WinHttpSetCredentials);
	RESOLVE(WinHttpSetDefaultProxyConfiguration);
	RESOLVE(WinHttpSetOption);
	RESOLVE(WinHttpSetStatusCallback);
	RESOLVE(WinHttpSetTimeouts);
	RESOLVE(WinHttpTimeFromSystemTime);
	RESOLVE(WinHttpTimeToSystemTime);
	RESOLVE(WinHttpUnregisterProxyChangeNotification);
	RESOLVE(WinHttpWriteData);
	RESOLVE(WinHttpWebSocketClose);
	RESOLVE(WinHttpWebSocketCompleteUpgrade);
	RESOLVE(WinHttpWebSocketQueryCloseStatus);
	RESOLVE(WinHttpWebSocketReceive);
	RESOLVE(WinHttpWebSocketSend);
	RESOLVE(WinHttpWebSocketShutdown);
#undef RESOLVE

	return true;
}
