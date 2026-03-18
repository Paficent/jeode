#pragma once

#include <windows.h>

extern "C" {
extern FARPROC orig_WinHttpOpen;
extern FARPROC orig_WinHttpConnect;
extern FARPROC orig_WinHttpSendRequest;
extern FARPROC orig_WinHttpCloseHandle;
extern FARPROC orig_WinHttpOpenRequest;
extern FARPROC orig_WinHttpReadData;
extern FARPROC orig_WinHttpQueryHeaders;
extern FARPROC orig_WinHttpAddRequestHeaders;
extern FARPROC orig_WinHttpQueryDataAvailable;
extern FARPROC orig_WinHttpReceiveResponse;
}

bool proxy_init_winhttp();
