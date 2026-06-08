#include "fs.h"
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
// static std::string s_mod_root;

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
	if (is_malicious(rel)) return "fs.read: malicious path detected";

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return "fs.read: path not found";
	if (fs::is_directory(resolved)) return "fs.read: path is a directory";

	std::error_code ec;
	auto size = fs::file_size(resolved, ec);
	if (ec) return "fs.read: could not determine file size";
	if (size > MAX_READ_SIZE) return "fs.read: file too large";

	std::ifstream f(resolved.string(), std::ios::binary);
	if (!f) return "fs.read: could not open file";

	out.resize(static_cast<size_t>(size));
	f.read(out.data(), static_cast<std::streamsize>(size));
	if (!f) return "fs.read: read error";

	return nullptr;
}

static const char *write_real(const char *rel, const char *data, size_t len) {
	if (is_malicious(rel)) return "fs.write: malicious path detected";

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return "fs.write: path outside sandbox";

	std::error_code ec;
	fs::create_directories(resolved.parent_path(), ec);
	if (ec) return "fs.write: could not create directories";
	if (!verify_parent(resolved)) return "fs.write: path outside sandbox";

	std::ofstream f(resolved.string(), std::ios::binary | std::ios::trunc);
	if (!f) return "fs.write: could not open file for writing";

	f.write(data, static_cast<std::streamsize>(len));
	if (!f) return "fs.write: write error";

	return nullptr;
}

static const char *append_real(const char *rel, const char *data, size_t len) {
	if (is_malicious(rel)) return "fs.append: malicious path detected";

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return "fs.append: path outside sandbox";

	std::error_code ec;
	fs::create_directories(resolved.parent_path(), ec);
	if (ec) return "fs.append: could not create directories";
	if (!verify_parent(resolved)) return "fs.append: path outside sandbox";

	std::ofstream f(resolved.string(), std::ios::binary | std::ios::app);
	if (!f) return "fs.append: could not open file for appending";

	f.write(data, static_cast<std::streamsize>(len));
	if (!f) return "fs.append: write error";

	return nullptr;
}

static const char *list_real(const char *rel, std::vector<std::string> &out) {
	if (is_malicious(rel)) return "fs.list: malicious path detected";

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return "fs.list: path not found";
	if (!fs::is_directory(resolved)) return "fs.list: path is not a directory";

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
	if (is_malicious(rel)) return "fs.makeFolder: malicious path detected";

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return "fs.makeFolder: path outside sandbox";

	std::error_code ec;
	fs::create_directories(resolved, ec);
	if (ec) return "fs.makeFolder: could not create directories";

	fs::path canon = fs::canonical(resolved, ec);
	if (ec) return "fs.makeFolder: verification failed";
	if (!is_under_root(canon.generic_string())) return "fs.makeFolder: path outside sandbox";

	return nullptr;
}

static const char *delfolder_real(const char *rel) {
	if (is_malicious(rel)) return "fs.deleteFolder: malicious path detected";
	if (!rel[0] || strcmp(rel, ".") == 0 || strcmp(rel, "./") == 0)
		return "fs.deleteFolder: cannot delete root directory";

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return "fs.deleteFolder: path not found or outside sandbox";
	if (!fs::is_directory(resolved)) return "fs.deleteFolder: not a directory";

	std::error_code ec;
	fs::remove_all(resolved, ec);
	if (ec) return "fs.deleteFolder: removal failed";

	return nullptr;
}

static const char *delfile_real(const char *rel) {
	if (is_malicious(rel)) return "fs.deleteFile: malicious path detected";

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return "fs.deleteFile: path not found";
	if (fs::is_directory(resolved)) return "fs.deleteFile: is a directory, use deleteFolder";

	std::error_code ec;
	fs::remove(resolved, ec);
	if (ec) return "fs.deleteFile: removal failed";

	return nullptr;
}

static int l_fs_read(lua_State *L) {
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

static int l_fs_write(lua_State *L) {
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

static int l_fs_append(lua_State *L) {
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

static int l_fs_list(lua_State *L) {
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

static int l_fs_isfile(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	bool result;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		isfile_real(rel, result);
	}

	lua_pushboolean(L, result);
	return 1;
}

static int l_fs_isfolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	bool result;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		isfolder_real(rel, result);
	}

	lua_pushboolean(L, result);
	return 1;
}

static int l_fs_makefolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = makefolder_real(rel);
	}

	if (err) return luaL_error(L, "%s", err);
	return 0;
}

static int l_fs_delfolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = delfolder_real(rel);
	}

	if (err) return luaL_error(L, "%s", err);
	return 0;
}

static int l_fs_delfile(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	const char *err;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		err = delfile_real(rel);
	}

	if (err) return luaL_error(L, "%s", err);
	return 0;
}

void fs_api_init(const char *gameDir) {
	spdlog::debug("[fs_api] early init began");

	std::lock_guard<std::mutex> lock(s_mutex);
	s_game_dir = gameDir;
	std::error_code ec;
	s_canon_root = fs::canonical(fs::path(gameDir), ec).generic_string();

	spdlog::debug("[fs_api] initialized (gameDir='{}')", gameDir);
}

static const LuaApiFunction FS_FUNCTIONS[] = {
	{"read", l_fs_read},
	{"write", l_fs_write},
	{"append", l_fs_append},
	{"list", l_fs_list},
	{"isFile", l_fs_isfile},
	{"isFolder", l_fs_isfolder},
	{"makeFolder", l_fs_makefolder},
	{"deleteFolder", l_fs_delfolder},
	{"deleteFile", l_fs_delfile},
};

static const LuaApiTable FS_TABLE = {
	"fs",
	FS_FUNCTIONS,
	sizeof(FS_FUNCTIONS) / sizeof(FS_FUNCTIONS[0]),
};

const LuaApiTable &fs_api_table() {
	return FS_TABLE;
}
