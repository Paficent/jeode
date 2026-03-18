.text

.global _proxy_WinHttpOpen
_proxy_WinHttpOpen:
    jmp *_orig_WinHttpOpen

.global _proxy_WinHttpConnect
_proxy_WinHttpConnect:
    jmp *_orig_WinHttpConnect

.global _proxy_WinHttpSendRequest
_proxy_WinHttpSendRequest:
    jmp *_orig_WinHttpSendRequest

.global _proxy_WinHttpCloseHandle
_proxy_WinHttpCloseHandle:
    jmp *_orig_WinHttpCloseHandle

.global _proxy_WinHttpOpenRequest
_proxy_WinHttpOpenRequest:
    jmp *_orig_WinHttpOpenRequest

.global _proxy_WinHttpReadData
_proxy_WinHttpReadData:
    jmp *_orig_WinHttpReadData

.global _proxy_WinHttpQueryHeaders
_proxy_WinHttpQueryHeaders:
    jmp *_orig_WinHttpQueryHeaders

.global _proxy_WinHttpAddRequestHeaders
_proxy_WinHttpAddRequestHeaders:
    jmp *_orig_WinHttpAddRequestHeaders

.global _proxy_WinHttpQueryDataAvailable
_proxy_WinHttpQueryDataAvailable:
    jmp *_orig_WinHttpQueryDataAvailable

.global _proxy_WinHttpReceiveResponse
_proxy_WinHttpReceiveResponse:
    jmp *_orig_WinHttpReceiveResponse
