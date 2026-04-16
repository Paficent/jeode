// This should be renamed to 'fs' in a later update
#include "file.h"
#include "../api.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::mutex s_mutex;
static std::string s_game_dir;
static std::string s_canon_root;
static std::string s_mod_root;

static constexpr size_t MAX_READ_SIZE = 64 * 1024 * 1024;

static const char *BAD_EXTENSIONS[] = {".exe",	  ".com", ".bat",  ".cmd",	".vbs",	 ".vbe", ".js",	 ".jse",  ".wsf",
									   ".wsh",	  ".ps1", ".psm1", ".psd1", ".psh1", ".msc", ".msh", ".msh1", ".msh2",
									   ".mshxml", ".dll", ".sys",  ".scr",	".cpl",	 ".inf", ".reg", ".cab",  ".psc1"};

static std::string str_lower(const std::string &s) {
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}

static bool is_malicious(const std::string &path) {
	if (path.find("..") != std::string::npos) return true;

	std::string lower = str_lower(path);
	for (const auto &ext : BAD_EXTENSIONS) {
		size_t elen = strlen(ext);
		if (lower.size() >= elen && lower.compare(lower.size() - elen, elen, ext) == 0) return true;
	}

	return false;
}

static bool is_under_root(const std::string &path) {
	std::string prefix = s_canon_root + "/";
	return path.size() >= prefix.size() && path.compare(0, prefix.size(), prefix) == 0;
}

// This sucks but is neccesary
static bool is_safe_relpath(const std::string &path) {
	if (path.empty()) return false;
	if (path[0] == '/' || path[0] == '\\') return false;
	if (path.size() >= 2 && path[1] == ':') return false;
	if (path.find("..") != std::string::npos) return false;
	if (path.find('\\') != std::string::npos) return false;
	return true;
}

// fs::weakly_canonical is cleaner
static fs::path safe_resolve(const std::string &relPath, bool must_exist) {
	if (!is_safe_relpath(relPath)) return {};

	fs::path candidate = fs::path(s_game_dir) / relPath;
	std::error_code ec;

	if (must_exist) {
		fs::path canon = fs::canonical(candidate, ec);
		if (ec) return {};
		if (!is_under_root(canon.generic_string())) return {};
		return canon;
	}

	fs::path canon = fs::weakly_canonical(candidate, ec);
	if (ec) return {};
	if (!is_under_root(canon.generic_string())) return {};
	return canon;
}

static bool verify_parent(const fs::path &path) {
	std::error_code ec;
	fs::path canon = fs::canonical(path.parent_path(), ec);
	if (ec) return false;
	std::string s = canon.generic_string();
	return s == s_canon_root || is_under_root(s);
}

// nullptr = success | error = failure
static const char *read_real(const char *rel, std::string &out) {
	if (is_malicious(rel)) return "file.read: malicious path detected";

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return "file.read: path not found";
	if (fs::is_directory(resolved)) return "file.read: path is a directory";

	std::error_code ec;
	auto size = fs::file_size(resolved, ec);
	if (ec) return "file.read: could not determine file size";
	if (size > MAX_READ_SIZE) return "file.read: file too large";

	std::ifstream f(resolved.string(), std::ios::binary);
	if (!f) return "file.read: could not open file";

	out.resize(static_cast<size_t>(size));
	f.read(out.data(), static_cast<std::streamsize>(size));
	if (!f) return "file.read: read error";

	return nullptr;
}

static const char *write_real(const char *rel, const char *data, size_t len) {
	if (is_malicious(rel)) return "file.write: malicious path detected";

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return "file.write: path outside sandbox";

	std::error_code ec;
	fs::create_directories(resolved.parent_path(), ec);
	if (ec) return "file.write: could not create directories";
	if (!verify_parent(resolved)) return "file.write: path outside sandbox";

	std::ofstream f(resolved.string(), std::ios::binary | std::ios::trunc);
	if (!f) return "file.write: could not open file for writing";

	f.write(data, static_cast<std::streamsize>(len));
	if (!f) return "file.write: write error";

	return nullptr;
}

static const char *append_real(const char *rel, const char *data, size_t len) {
	if (is_malicious(rel)) return "file.append: malicious path detected";

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return "file.append: path outside sandbox";

	std::error_code ec;
	fs::create_directories(resolved.parent_path(), ec);
	if (ec) return "file.append: could not create directories";
	if (!verify_parent(resolved)) return "file.append: path outside sandbox";

	std::ofstream f(resolved.string(), std::ios::binary | std::ios::app);
	if (!f) return "file.append: could not open file for appending";

	f.write(data, static_cast<std::streamsize>(len));
	if (!f) return "file.append: write error";

	return nullptr;
}

static const char *list_real(const char *rel, std::vector<std::string> &out) {
	if (is_malicious(rel)) return "file.list: malicious path detected";

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return "file.list: path not found";
	if (!fs::is_directory(resolved)) return "file.list: path is not a directory";

	std::string base(rel);
	while (!base.empty() && base.back() == '/') base.pop_back();

	for (const auto &entry : fs::directory_iterator(resolved))
		out.push_back(base + "/" + entry.path().filename().string());

	return nullptr;
}

static const char *isfile_real(const char *rel, bool &out) {
	out = false;
	if (is_malicious(rel)) return nullptr;

	fs::path resolved = safe_resolve(rel, true);
	out = !resolved.empty() && fs::exists(resolved) && !fs::is_directory(resolved);
	return nullptr;
}

static const char *isfolder_real(const char *rel, bool &out) {
	out = false;
	if (is_malicious(rel)) return nullptr;

	fs::path resolved = safe_resolve(rel, true);
	out = !resolved.empty() && fs::is_directory(resolved);
	return nullptr;
}

static const char *makefolder_real(const char *rel) {
	if (is_malicious(rel)) return "file.makeFolder: malicious path detected";

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return "file.makeFolder: path outside sandbox";

	std::error_code ec;
	fs::create_directories(resolved, ec);
	if (ec) return "file.makeFolder: could not create directories";

	fs::path canon = fs::canonical(resolved, ec);
	if (ec) return "file.makeFolder: verification failed";
	if (!is_under_root(canon.generic_string())) return "file.makeFolder: path outside sandbox";

	return nullptr;
}

static const char *delfolder_real(const char *rel) {
	if (is_malicious(rel)) return "file.deleteFolder: malicious path detected";
	if (!rel[0] || strcmp(rel, ".") == 0 || strcmp(rel, "./") == 0)
		return "file.deleteFolder: cannot delete root directory";

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return "file.deleteFolder: path not found or outside sandbox";
	if (!fs::is_directory(resolved)) return "file.deleteFolder: not a directory";

	std::error_code ec;
	fs::remove_all(resolved, ec);
	if (ec) return "file.deleteFolder: removal failed";

	return nullptr;
}

static const char *delfile_real(const char *rel) {
	if (is_malicious(rel)) return "file.deleteFile: malicious path detected";

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return "file.deleteFile: path not found";
	if (fs::is_directory(resolved)) return "file.deleteFile: is a directory, use deleteFolder";

	std::error_code ec;
	fs::remove(resolved, ec);
	if (ec) return "file.deleteFile: removal failed";

	return nullptr;
}

static int l_file_read(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	std::string content;
	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = read_real(rel, content);
	}

	if (err) return luaL_error(L, "%s", err);
	lua_pushlstring(L, content.data(), content.size());
	return 1;
}

static int l_file_write(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);
	size_t len = 0;
	const char *data = luaL_checklstring(L, 2, &len);

	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = write_real(rel, data, len);
	}

	if (err) return luaL_error(L, "%s", err);
	return 0;
}

static int l_file_append(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);
	size_t len = 0;
	const char *data = luaL_checklstring(L, 2, &len);

	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = append_real(rel, data, len);
	}

	if (err) return luaL_error(L, "%s", err);
	return 0;
}

static int l_file_list(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	std::vector<std::string> entries;
	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = list_real(rel, entries);
	}

	if (err) return luaL_error(L, "%s", err);

	lua_newtable(L);
	for (size_t i = 0; i < entries.size(); i++) {
		lua_pushstring(L, entries[i].c_str());
		lua_rawseti(L, -2, static_cast<int>(i + 1));
	}
	return 1;
}

static int l_file_isfile(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	bool result;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		isfile_real(rel, result);
	}

	lua_pushboolean(L, result);
	return 1;
}

static int l_file_isfolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	bool result;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		isfolder_real(rel, result);
	}

	lua_pushboolean(L, result);
	return 1;
}

static int l_file_makefolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = makefolder_real(rel);
	}

	if (err) return luaL_error(L, "%s", err);
	return 0;
}

static int l_file_delfolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = delfolder_real(rel);
	}

	if (err) return luaL_error(L, "%s", err);
	return 0;
}

static int l_file_delfile(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = delfile_real(rel);
	}

	if (err) return luaL_error(L, "%s", err);
	return 0;
}

void file_api_init(const char *gameDir) {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_game_dir = gameDir;
	std::error_code ec;
	s_canon_root = fs::canonical(fs::path(gameDir), ec).generic_string();

	spdlog::debug("[file_api] initialized (gameDir='{}')", gameDir);
}

static const LuaApiFunction FILE_FUNCTIONS[] = {
	{"read", l_file_read},
	{"write", l_file_write},
	{"append", l_file_append},
	{"list", l_file_list},
	{"isFile", l_file_isfile},
	{"isFolder", l_file_isfolder},
	{"makeFolder", l_file_makefolder},
	{"deleteFolder", l_file_delfolder},
	{"deleteFile", l_file_delfile},
};

static const LuaApiTable FILE_TABLE = {
	"file",
	FILE_FUNCTIONS,
	sizeof(FILE_FUNCTIONS) / sizeof(FILE_FUNCTIONS[0]),
};

const LuaApiTable &file_api_table() {
	return FILE_TABLE;
}

// These are stubs, it's probably best to keep fs api stuck to the game path
void file_api_set_mod_root(const char *path) {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_mod_root = path ? path : "";
}

void file_api_clear_mod_root() {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_mod_root.clear();
}
