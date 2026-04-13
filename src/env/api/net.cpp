#include "net.h"
#include "../api.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

using json = nlohmann::json;

struct RequestParams {
	std::string url;
	std::string method;
	std::string body;
	std::vector<std::pair<std::string, std::string>> headers;
	std::vector<std::pair<std::string, std::string>> query;
};

struct ResponseData {
	int status_code;
	std::string status_message;
	std::vector<std::pair<std::string, std::string>> headers;
	std::string body;
};

struct WinHttpSession {
	HINTERNET session = nullptr;
	HINTERNET connection = nullptr;
	HINTERNET request = nullptr;

	~WinHttpSession() {
		if (request) WinHttpCloseHandle(request);
		if (connection) WinHttpCloseHandle(connection);
		if (session) WinHttpCloseHandle(session);
	}
};

// TODO: utilities library since I know for a fact I'm using this in file hooks
static std::wstring utf8_to_wide(const std::string &utf8) {
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
	if (len <= 0) return {};
	std::wstring out(len - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], len);
	return out;
}

// TODO: utilities library since I know for a fact I'm using this in file hooks
static std::string wide_to_utf8(const wchar_t *wide) {
	int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
	if (len <= 0) return {};
	std::string out(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, &out[0], len, nullptr, nullptr);
	return out;
}

// This is awful, the things I do for Lune-like API's
static const char *status_message(int code) {
	switch (code) {
	case 100:
		return "Continue";
	case 101:
		return "Switching Protocols";
	case 200:
		return "OK";
	case 201:
		return "Created";
	case 202:
		return "Accepted";
	case 203:
		return "Non-Authoritative Information";
	case 204:
		return "No Content";
	case 205:
		return "Reset Content";
	case 206:
		return "Partial Content";
	case 300:
		return "Multiple Choices";
	case 301:
		return "Moved Permanently";
	case 302:
		return "Found";
	case 303:
		return "See Other";
	case 304:
		return "Not Modified";
	case 307:
		return "Temporary Redirect";
	case 308:
		return "Permanent Redirect";
	case 400:
		return "Bad Request";
	case 401:
		return "Unauthorized";
	case 403:
		return "Forbidden";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 406:
		return "Not Acceptable";
	case 408:
		return "Request Timeout";
	case 409:
		return "Conflict";
	case 410:
		return "Gone";
	case 413:
		return "Payload Too Large";
	case 415:
		return "Unsupported Media Type";
	case 422:
		return "Unprocessable Entity";
	case 429:
		return "Too Many Requests";
	case 500:
		return "Internal Server Error";
	case 502:
		return "Bad Gateway";
	case 503:
		return "Service Unavailable";
	case 504:
		return "Gateway Timeout";
	default:
		return "Unknown";
	}
}

static std::string url_encode(const char *data, size_t len, bool binary) {
	std::string out;
	out.reserve(len);
	for (size_t i = 0; i < len; i++) {
		unsigned char c = static_cast<unsigned char>(data[i]);
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			out += static_cast<char>(c);
			continue;
		}
		if (!binary && c == ' ') {
			out += '+';
			continue;
		}
		char buf[4];
		snprintf(buf, sizeof(buf), "%%%02X", c);
		out += buf;
	}
	return out;
}

static int hex_digit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static std::string url_decode(const char *data, size_t len, bool binary) {
	std::string out;
	out.reserve(len);
	for (size_t i = 0; i < len; i++) {
		if (data[i] == '%' && i + 2 < len) {
			int hi = hex_digit(data[i + 1]);
			int lo = hex_digit(data[i + 2]);
			if (hi >= 0 && lo >= 0) {
				out += static_cast<char>((hi << 4) | lo);
				i += 2;
				continue;
			}
		}
		if (!binary && data[i] == '+') {
			out += ' ';
			continue;
		}
		out += data[i];
	}
	return out;
}

static std::string build_query_url(const RequestParams &params) {
	if (params.query.empty()) return params.url;

	std::string url = params.url;
	url += (url.find('?') == std::string::npos) ? '?' : '&';

	for (size_t i = 0; i < params.query.size(); i++) {
		if (i > 0) url += '&';
		const auto &k = params.query[i].first;
		const auto &v = params.query[i].second;
		url += url_encode(k.data(), k.size(), true) + "=" + url_encode(v.data(), v.size(), true);
	}

	return url;
}

static void read_string_table(lua_State *L, int table_index, std::vector<std::pair<std::string, std::string>> &out) {
	lua_pushnil(L);
	while (lua_next(L, table_index) != 0) {
		if (lua_isstring(L, -2) && lua_isstring(L, -1)) out.emplace_back(lua_tostring(L, -2), lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

static RequestParams parse_request(lua_State *L) {
	RequestParams params;
	params.method = "GET";

	if (lua_isstring(L, 1)) {
		params.url = lua_tostring(L, 1);
		return params;
	}

	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "url");
	if (!lua_isstring(L, -1)) luaL_error(L, "net.request: 'url' is required");
	params.url = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "method");
	if (lua_isstring(L, -1)) params.method = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "body");
	if (lua_isstring(L, -1)) {
		size_t len;
		const char *data = lua_tolstring(L, -1, &len);
		params.body.assign(data, len);
	}
	lua_pop(L, 1);

	lua_getfield(L, 1, "headers");
	if (lua_istable(L, -1)) read_string_table(L, lua_gettop(L), params.headers);
	lua_pop(L, 1);

	lua_getfield(L, 1, "query");
	if (lua_istable(L, -1)) read_string_table(L, lua_gettop(L), params.query);
	lua_pop(L, 1);

	return params;
}

static void parse_response_headers(const std::string &raw, std::vector<std::pair<std::string, std::string>> &out) {
	size_t pos = raw.find("\r\n");
	if (pos == std::string::npos) return;
	pos += 2;

	while (pos < raw.size()) {
		size_t end = raw.find("\r\n", pos);
		if (end == std::string::npos || end == pos) break;

		size_t colon = raw.find(':', pos);
		if (colon == std::string::npos || colon >= end) {
			pos = end + 2;
			continue;
		}

		std::string key = raw.substr(pos, colon - pos);
		for (auto &c : key) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

		size_t value_start = colon + 1;
		while (value_start < end && raw[value_start] == ' ') ++value_start;

		out.emplace_back(std::move(key), raw.substr(value_start, end - value_start));
		pos = end + 2;
	}
}

static std::string perform_request(const RequestParams &params, ResponseData &response) {
	std::wstring wide_url = utf8_to_wide(build_query_url(params));

	URL_COMPONENTS uc = {};
	uc.dwStructSize = sizeof(uc);
	uc.dwSchemeLength = static_cast<DWORD>(-1);
	uc.dwHostNameLength = static_cast<DWORD>(-1);
	uc.dwUrlPathLength = static_cast<DWORD>(-1);
	uc.dwExtraInfoLength = static_cast<DWORD>(-1);

	if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &uc)) return "net.request: invalid URL";

	std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
	std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
	if (uc.dwExtraInfoLength > 0) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

	WinHttpSession http;

	http.session = WinHttpOpen(L"jeode/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
							   WINHTTP_NO_PROXY_BYPASS, 0);
	if (!http.session) return "net.request: failed to open session";

	http.connection = WinHttpConnect(http.session, host.c_str(), uc.nPort, 0);
	if (!http.connection) return "net.request: failed to connect";

	DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
	std::wstring wide_method = utf8_to_wide(params.method);

	http.request = WinHttpOpenRequest(http.connection, wide_method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
									  WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!http.request) return "net.request: failed to create request";

	for (const auto &[key, value] : params.headers) {
		std::wstring header = utf8_to_wide(key + ": " + value);
		WinHttpAddRequestHeaders(http.request, header.c_str(), static_cast<DWORD>(-1),
								 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
	}

	LPVOID body_ptr = params.body.empty() ? WINHTTP_NO_REQUEST_DATA
										  : const_cast<LPVOID>(static_cast<const void *>(params.body.data()));
	DWORD body_len = static_cast<DWORD>(params.body.size());

	if (!WinHttpSendRequest(http.request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body_ptr, body_len, body_len, 0))
		return "net.request: send failed";

	if (!WinHttpReceiveResponse(http.request, nullptr)) return "net.request: receive failed";

	DWORD status_code = 0;
	DWORD dword_size = sizeof(status_code);
	WinHttpQueryHeaders(http.request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
						WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &dword_size, WINHTTP_NO_HEADER_INDEX);
	response.status_code = static_cast<int>(status_code);
	response.status_message = status_message(response.status_code);

	DWORD header_size = 0;
	WinHttpQueryHeaders(http.request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr,
						&header_size, WINHTTP_NO_HEADER_INDEX);

	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && header_size > 0) {
		std::vector<wchar_t> buf(header_size / sizeof(wchar_t));
		if (WinHttpQueryHeaders(http.request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, buf.data(),
								&header_size, WINHTTP_NO_HEADER_INDEX))
			parse_response_headers(wide_to_utf8(buf.data()), response.headers);
	}

	DWORD available = 0;
	while (WinHttpQueryDataAvailable(http.request, &available) && available > 0) {
		std::vector<char> buf(available);
		DWORD bytes_read = 0;
		if (!WinHttpReadData(http.request, buf.data(), available, &bytes_read)) break;
		response.body.append(buf.data(), bytes_read);
	}

	return {};
}

static void push_response(lua_State *L, const ResponseData &response) {
	lua_newtable(L);

	lua_pushboolean(L, response.status_code >= 200 && response.status_code < 300);
	lua_setfield(L, -2, "ok");

	lua_pushinteger(L, response.status_code);
	lua_setfield(L, -2, "statusCode");

	lua_pushstring(L, response.status_message.c_str());
	lua_setfield(L, -2, "statusMessage");

	lua_newtable(L);
	for (const auto &[key, value] : response.headers) {
		lua_pushstring(L, value.c_str());
		lua_setfield(L, -2, key.c_str());
	}
	lua_setfield(L, -2, "headers");

	lua_pushlstring(L, response.body.data(), response.body.size());
	lua_setfield(L, -2, "body");
}

static json lua_to_json(lua_State *L, int index);

static json lua_table_to_json(lua_State *L, int index) {
	int abs = (index > 0) ? index : lua_gettop(L) + index + 1;

	bool is_array = true;
	int max_key = 0;
	int count = 0;

	lua_pushnil(L);
	while (lua_next(L, abs) != 0) {
		if (lua_type(L, -2) != LUA_TNUMBER || lua_tointeger(L, -2) != static_cast<lua_Integer>(lua_tonumber(L, -2))) {
			is_array = false;
			lua_pop(L, 2);
			break;
		}
		int k = static_cast<int>(lua_tointeger(L, -2));
		if (k < 1) {
			is_array = false;
			lua_pop(L, 2);
			break;
		}
		if (k > max_key) max_key = k;
		count++;
		lua_pop(L, 1);
	}

	if (is_array && max_key == count) {
		json arr = json::array();
		for (int i = 1; i <= max_key; i++) {
			lua_rawgeti(L, abs, i);
			arr.push_back(lua_to_json(L, -1));
			lua_pop(L, 1);
		}
		return arr;
	}

	json obj = json::object();
	lua_pushnil(L);
	while (lua_next(L, abs) != 0) {
		std::string key;
		if (lua_type(L, -2) == LUA_TSTRING)
			key = lua_tostring(L, -2);
		else if (lua_type(L, -2) == LUA_TNUMBER)
			key = std::to_string(static_cast<long long>(lua_tointeger(L, -2)));
		else {
			lua_pop(L, 1);
			continue;
		}
		obj[key] = lua_to_json(L, -1);
		lua_pop(L, 1);
	}
	return obj;
}

static json lua_to_json(lua_State *L, int index) {
	switch (lua_type(L, index)) {
	case LUA_TNIL:
		return nullptr;
	case LUA_TBOOLEAN:
		return lua_toboolean(L, index) != 0;
	case LUA_TNUMBER: {
		lua_Number n = lua_tonumber(L, index);
		lua_Integer i = lua_tointeger(L, index);
		if (static_cast<lua_Number>(i) == n) return i;
		return n;
	}
	case LUA_TSTRING: {
		size_t len;
		const char *s = lua_tolstring(L, index, &len);
		return std::string(s, len);
	}
	case LUA_TTABLE:
		return lua_table_to_json(L, index);
	default:
		return nullptr;
	}
}

static void json_to_lua(lua_State *L, const json &j) {
	if (j.is_null()) {
		lua_pushnil(L);
		return;
	}
	if (j.is_boolean()) {
		lua_pushboolean(L, j.get<bool>());
		return;
	}
	if (j.is_number_integer()) {
		lua_pushinteger(L, j.get<lua_Integer>());
		return;
	}
	if (j.is_number_float()) {
		lua_pushnumber(L, j.get<lua_Number>());
		return;
	}
	if (j.is_string()) {
		const auto &s = j.get_ref<const std::string &>();
		lua_pushlstring(L, s.data(), s.size());
		return;
	}
	if (j.is_array()) {
		lua_createtable(L, static_cast<int>(j.size()), 0);
		for (size_t i = 0; i < j.size(); i++) {
			json_to_lua(L, j[i]);
			lua_rawseti(L, -2, static_cast<int>(i + 1));
		}
		return;
	}
	if (j.is_object()) {
		lua_createtable(L, 0, static_cast<int>(j.size()));
		for (auto it = j.begin(); it != j.end(); ++it) {
			lua_pushlstring(L, it.key().data(), it.key().size());
			json_to_lua(L, it.value());
			lua_rawset(L, -3);
		}
		return;
	}
	lua_pushnil(L);
}

static int l_request(lua_State *L) {
	RequestParams params = parse_request(L);
	ResponseData response;

	std::string error = perform_request(params, response);
	if (!error.empty()) return luaL_error(L, "%s", error.c_str());

	push_response(L, response);
	return 1;
}

static int l_url_encode(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	bool binary = lua_isboolean(L, 2) && lua_toboolean(L, 2);

	std::string encoded = url_encode(data, len, binary);
	lua_pushlstring(L, encoded.data(), encoded.size());
	return 1;
}

static int l_url_decode(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	bool binary = lua_isboolean(L, 2) && lua_toboolean(L, 2);

	std::string decoded = url_decode(data, len, binary);
	lua_pushlstring(L, decoded.data(), decoded.size());
	return 1;
}

static int l_json_encode(lua_State *L) {
	luaL_checkany(L, 1);
	try {
		json j = lua_to_json(L, 1);
		std::string out = j.dump();
		lua_pushlstring(L, out.data(), out.size());
		return 1;
	} catch (const std::exception &e) {
		return luaL_error(L, "net.jsonEncode: %s", e.what());
	}
}

static int l_json_decode(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	try {
		json j = json::parse(data, data + len);
		json_to_lua(L, j);
		return 1;
	} catch (const std::exception &e) {
		return luaL_error(L, "net.jsonDecode: %s", e.what());
	}
}

static const LuaApiFunction NET_FUNCTIONS[] = {
	{"request", l_request},		   {"urlEncode", l_url_encode},	  {"urlDecode", l_url_decode},
	{"jsonEncode", l_json_encode}, {"jsonDecode", l_json_decode},
};

static const LuaApiTable NET_TABLE = {
	"net",
	NET_FUNCTIONS,
	sizeof(NET_FUNCTIONS) / sizeof(NET_FUNCTIONS[0]),
};

const LuaApiTable &net_api_table() {
	return NET_TABLE;
}
