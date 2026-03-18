#include "file.h"
#include "../../lua/game_lua.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string s_game_dir;
static std::string s_canon_root;
static std::string s_mod_root;

static const char *BAD_EXTENSIONS[] = {
	".exe", ".com",	 ".bat",  ".cmd",  ".vbs", ".vbe", ".js",	".jse",	 ".wsf",	".wsh",
	".ps1", ".psm1", ".psd1", ".psh1", ".msc", ".msh", ".msh1", ".msh2", ".mshxml",
};

static bool is_malicious(const std::string &path) {
	if (path.find("../") != std::string::npos || path.find("..\\") != std::string::npos) return true;

	for (const auto &ext : BAD_EXTENSIONS) {
		size_t elen = strlen(ext);
		if (path.size() >= elen && path.compare(path.size() - elen, elen, ext) == 0) return true;
	}

	return false;
}

static bool is_under_root(const std::string &path) {
	std::string prefix = s_canon_root + "/";
	return path.size() >= prefix.size() && path.compare(0, prefix.size(), prefix) == 0;
}

static bool is_safe_relpath(const std::string &path) {
	if (path.empty()) return false;
	if (path[0] == '/' || path[0] == '\\') return false;
	if (path.size() >= 2 && path[1] == ':') return false;
	if (path.find("..") != std::string::npos) return false;
	if (path.find('\\') != std::string::npos) return false;
	return true;
}

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

	fs::path cur = candidate;
	std::vector<fs::path> parts;
	while (!cur.empty() && cur != cur.root_path()) {
		if (fs::exists(cur, ec)) break;
		parts.push_back(cur.filename());
		cur = cur.parent_path();
	}

	if (cur.empty()) cur = fs::path(s_game_dir);

	fs::path canonAncestor = fs::canonical(cur, ec);
	if (ec) return {};

	std::string ancStr = canonAncestor.generic_string();
	if (ancStr != s_canon_root && !is_under_root(ancStr)) return {};

	fs::path result = canonAncestor;
	for (auto it = parts.rbegin(); it != parts.rend(); ++it) result /= *it;
	return result;
}

static int l_file_read(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	if (is_malicious(rel)) return luaL_error(L, "file.read: malicious path detected");

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return luaL_error(L, "file.read: path not found");
	if (fs::is_directory(resolved)) return luaL_error(L, "file.read: path is a directory");

	std::ifstream f(resolved.string(), std::ios::binary);
	if (!f) return luaL_error(L, "file.read: could not open file");

	std::ostringstream ss;
	ss << f.rdbuf();
	std::string content = ss.str();
	lua_pushlstring(L, content.c_str(), content.size());
	return 1;
}

static int l_file_write(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);
	size_t len = 0;
	const char *data = luaL_checklstring(L, 2, &len);

	if (is_malicious(rel)) return luaL_error(L, "file.write: malicious path detected");

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return luaL_error(L, "file.write: path outside sandbox");

	std::error_code ec;
	fs::create_directories(resolved.parent_path(), ec);

	std::ofstream f(resolved.string(), std::ios::binary | std::ios::trunc);
	if (!f) return luaL_error(L, "file.write: could not open file for writing");

	f.write(data, static_cast<std::streamsize>(len));
	return 0;
}

static int l_file_append(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);
	size_t len = 0;
	const char *data = luaL_checklstring(L, 2, &len);

	if (is_malicious(rel)) return luaL_error(L, "file.append: malicious path detected");

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return luaL_error(L, "file.append: path outside sandbox");

	std::error_code ec;
	fs::create_directories(resolved.parent_path(), ec);

	std::ofstream f(resolved.string(), std::ios::binary | std::ios::app);
	if (!f) return luaL_error(L, "file.append: could not open file for appending");

	f.write(data, static_cast<std::streamsize>(len));
	return 0;
}

static int l_file_list(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	if (is_malicious(rel)) return luaL_error(L, "file.list: malicious path detected");

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return luaL_error(L, "file.list: path not found");
	if (!fs::is_directory(resolved)) return luaL_error(L, "file.list: path is not a directory");

	lua_newtable(L);
	int idx = 1;
	for (const auto &entry : fs::directory_iterator(resolved)) {
		std::string full = std::string(rel) + "/" + entry.path().filename().string();
		lua_pushstring(L, full.c_str());
		lua_rawseti(L, -2, idx++);
	}
	return 1;
}

static int l_file_isfile(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);
	if (is_malicious(rel)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	fs::path resolved = safe_resolve(rel, true);
	lua_pushboolean(L, !resolved.empty() && fs::exists(resolved) && !fs::is_directory(resolved));
	return 1;
}

static int l_file_isfolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);
	if (is_malicious(rel)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	fs::path resolved = safe_resolve(rel, true);
	lua_pushboolean(L, !resolved.empty() && fs::is_directory(resolved));
	return 1;
}

static int l_file_makefolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	if (is_malicious(rel)) return luaL_error(L, "file.makeFolder: malicious path detected");

	fs::path resolved = safe_resolve(rel, false);
	if (resolved.empty()) return luaL_error(L, "file.makeFolder: path outside sandbox");

	std::error_code ec;
	fs::create_directories(resolved, ec);
	if (ec) return luaL_error(L, "file.makeFolder: %s", ec.message().c_str());
	return 0;
}

static int l_file_delfolder(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	if (is_malicious(rel)) return luaL_error(L, "file.deleteFolder: malicious path detected");
	if (!rel[0] || strcmp(rel, ".") == 0 || strcmp(rel, "./") == 0)
		return luaL_error(L, "file.deleteFolder: cannot delete root directory");

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return luaL_error(L, "file.deleteFolder: path not found or outside sandbox");
	if (!fs::is_directory(resolved)) return luaL_error(L, "file.deleteFolder: not a directory");

	std::error_code ec;
	fs::remove_all(resolved, ec);
	if (ec) return luaL_error(L, "file.deleteFolder: %s", ec.message().c_str());
	return 0;
}

static int l_file_delfile(lua_State *L) {
	const char *rel = luaL_checkstring(L, 1);

	if (is_malicious(rel)) return luaL_error(L, "file.deleteFile: malicious path detected");

	fs::path resolved = safe_resolve(rel, true);
	if (resolved.empty()) return luaL_error(L, "file.deleteFile: path not found");
	if (fs::is_directory(resolved)) return luaL_error(L, "file.deleteFile: is a directory, use deleteFolder");

	std::error_code ec;
	fs::remove(resolved, ec);
	if (ec) return luaL_error(L, "file.deleteFile: %s", ec.message().c_str());
	return 0;
}

// Phase 1: register C functions as globals and set up state
void file_api_init(lua_State *L, const char *gameDir) {
	s_game_dir = gameDir;
	std::error_code ec;
	s_canon_root = fs::canonical(fs::path(gameDir), ec).generic_string();

	spdlog::debug("[env] registering file API (gameDir='{}')...", gameDir);

	lua_pushcfunction(L, l_file_read);
	lua_setglobal(L, "__file_read");
	lua_pushcfunction(L, l_file_write);
	lua_setglobal(L, "__file_write");
	lua_pushcfunction(L, l_file_append);
	lua_setglobal(L, "__file_append");
	lua_pushcfunction(L, l_file_list);
	lua_setglobal(L, "__file_list");
	lua_pushcfunction(L, l_file_isfile);
	lua_setglobal(L, "__file_isFile");
	lua_pushcfunction(L, l_file_isfolder);
	lua_setglobal(L, "__file_isFolder");
	lua_pushcfunction(L, l_file_makefolder);
	lua_setglobal(L, "__file_makeFolder");
	lua_pushcfunction(L, l_file_delfolder);
	lua_setglobal(L, "__file_deleteFolder");
	lua_pushcfunction(L, l_file_delfile);
	lua_setglobal(L, "__file_deleteFile");
}

// Phase 2: build the file table in Lua and clean up temp globals
static const char API_BUILD_LUA[] = R"LUA(
file = {
    read         = __file_read,
    write        = __file_write,
    append       = __file_append,
    list         = __file_list,
    isFile       = __file_isFile,
    isFolder     = __file_isFolder,
    makeFolder   = __file_makeFolder,
    deleteFolder = __file_deleteFolder,
    deleteFile   = __file_deleteFile,
}
__file_read         = nil
__file_write        = nil
__file_append       = nil
__file_list         = nil
__file_isFile       = nil
__file_isFolder     = nil
__file_makeFolder   = nil
__file_deleteFolder = nil
__file_deleteFile   = nil
)LUA";

void file_api_build_table(lua_State *L) {
	int base = lua_gettop(L);
	int s = game_luaL_loadbuffer(L, API_BUILD_LUA, static_cast<int>(strlen(API_BUILD_LUA)), "=file_api_init");
	if (s == 0) game_lua_pcall(L, 0, 0, 0);
	game_lua_settop(L, base);
}

void file_api_set_mod_root(const char *path) {
	s_mod_root = path ? path : "";
}

void file_api_clear_mod_root() {
	s_mod_root.clear();
}
