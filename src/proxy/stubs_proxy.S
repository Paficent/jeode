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

.global _proxy_WinHttpAddRequestHeadersEx
_proxy_WinHttpAddRequestHeadersEx:
    jmp *_orig_WinHttpAddRequestHeadersEx

.global _proxy_WinHttpCheckPlatform
_proxy_WinHttpCheckPlatform:
    jmp *_orig_WinHttpCheckPlatform

.global _proxy_WinHttpCrackUrl
_proxy_WinHttpCrackUrl:
    jmp *_orig_WinHttpCrackUrl

.global _proxy_WinHttpCreateProxyResolver
_proxy_WinHttpCreateProxyResolver:
    jmp *_orig_WinHttpCreateProxyResolver

.global _proxy_WinHttpCreateUrl
_proxy_WinHttpCreateUrl:
    jmp *_orig_WinHttpCreateUrl

.global _proxy_WinHttpDetectAutoProxyConfigUrl
_proxy_WinHttpDetectAutoProxyConfigUrl:
    jmp *_orig_WinHttpDetectAutoProxyConfigUrl

.global _proxy_WinHttpFreeProxyResult
_proxy_WinHttpFreeProxyResult:
    jmp *_orig_WinHttpFreeProxyResult

.global _proxy_WinHttpFreeProxySettingsEx
_proxy_WinHttpFreeProxySettingsEx:
    jmp *_orig_WinHttpFreeProxySettingsEx

.global _proxy_WinHttpFreeQueryConnectionGroupResult
_proxy_WinHttpFreeQueryConnectionGroupResult:
    jmp *_orig_WinHttpFreeQueryConnectionGroupResult

.global _proxy_WinHttpGetDefaultProxyConfiguration
_proxy_WinHttpGetDefaultProxyConfiguration:
    jmp *_orig_WinHttpGetDefaultProxyConfiguration

.global _proxy_WinHTTPGetIEProxyConfigForCurrentUser
_proxy_WinHTTPGetIEProxyConfigForCurrentUser:
    jmp *_orig_WinHTTPGetIEProxyConfigForCurrentUser

.global _proxy_WinHttpGetProxyForUrl
_proxy_WinHttpGetProxyForUrl:
    jmp *_orig_WinHttpGetProxyForUrl

.global _proxy_WinHttpGetProxyForUrlEx
_proxy_WinHttpGetProxyForUrlEx:
    jmp *_orig_WinHttpGetProxyForUrlEx

.global _proxy_WinHttpGetProxyResult
_proxy_WinHttpGetProxyResult:
    jmp *_orig_WinHttpGetProxyResult

.global _proxy_WinHttpGetProxySettingsEx
_proxy_WinHttpGetProxySettingsEx:
    jmp *_orig_WinHttpGetProxySettingsEx

.global _proxy_WinHttpGetProxySettingsResultEx
_proxy_WinHttpGetProxySettingsResultEx:
    jmp *_orig_WinHttpGetProxySettingsResultEx

.global _proxy_WinHttpQueryAuthSchemes
_proxy_WinHttpQueryAuthSchemes:
    jmp *_orig_WinHttpQueryAuthSchemes

.global _proxy_WinHttpQueryConnectionGroup
_proxy_WinHttpQueryConnectionGroup:
    jmp *_orig_WinHttpQueryConnectionGroup

.global _proxy_WinHttpQueryHeadersEx
_proxy_WinHttpQueryHeadersEx:
    jmp *_orig_WinHttpQueryHeadersEx

.global _proxy_WinHttpQueryOption
_proxy_WinHttpQueryOption:
    jmp *_orig_WinHttpQueryOption

.global _proxy_WinHttpReadDataEx
_proxy_WinHttpReadDataEx:
    jmp *_orig_WinHttpReadDataEx
.global _proxy_WinHttpRegisterProxyChangeNotification

_proxy_WinHttpRegisterProxyChangeNotification:
    jmp *_orig_WinHttpRegisterProxyChangeNotification

.global _proxy_WinHttpResetAutoProxy
_proxy_WinHttpResetAutoProxy:
    jmp *_orig_WinHttpResetAutoProxy

.global _proxy_WinHttpSetCredentials
_proxy_WinHttpSetCredentials:
    jmp *_orig_WinHttpSetCredentials
.global _proxy_WinHttpSetDefaultProxyConfiguration
_proxy_WinHttpSetDefaultProxyConfiguration:
    jmp *_orig_WinHttpSetDefaultProxyConfiguration

.global _proxy_WinHttpSetOption
_proxy_WinHttpSetOption:
    jmp *_orig_WinHttpSetOption

.global _proxy_WinHttpSetStatusCallback
_proxy_WinHttpSetStatusCallback:
    jmp *_orig_WinHttpSetStatusCallback

.global _proxy_WinHttpSetTimeouts
_proxy_WinHttpSetTimeouts:
    jmp *_orig_WinHttpSetTimeouts

.global _proxy_WinHttpTimeFromSystemTime
_proxy_WinHttpTimeFromSystemTime:
    jmp *_orig_WinHttpTimeFromSystemTime

.global _proxy_WinHttpTimeToSystemTime
_proxy_WinHttpTimeToSystemTime:
    jmp *_orig_WinHttpTimeToSystemTime

.global _proxy_WinHttpUnregisterProxyChangeNotification
_proxy_WinHttpUnregisterProxyChangeNotification:
    jmp *_orig_WinHttpUnregisterProxyChangeNotification

.global _proxy_WinHttpWriteData
_proxy_WinHttpWriteData:
    jmp *_orig_WinHttpWriteData

.global _proxy_WinHttpWebSocketClose
_proxy_WinHttpWebSocketClose:
    jmp *_orig_WinHttpWebSocketClose

.global _proxy_WinHttpWebSocketCompleteUpgrade
_proxy_WinHttpWebSocketCompleteUpgrade:
    jmp *_orig_WinHttpWebSocketCompleteUpgrade

.global _proxy_WinHttpWebSocketQueryCloseStatus
_proxy_WinHttpWebSocketQueryCloseStatus:
    jmp *_orig_WinHttpWebSocketQueryCloseStatus

.global _proxy_WinHttpWebSocketReceive
_proxy_WinHttpWebSocketReceive:
    jmp *_orig_WinHttpWebSocketReceive

.global _proxy_WinHttpWebSocketSend
_proxy_WinHttpWebSocketSend:
    jmp *_orig_WinHttpWebSocketSend

.global _proxy_WinHttpWebSocketShutdown
_proxy_WinHttpWebSocketShutdown:
    jmp *_orig_WinHttpWebSocketShutdown
