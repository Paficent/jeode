#include "environment.h"
#include "../core/overlay.h"
#include "../core/version.h"
#include "../lua/game_lua.h"
#include "../lua/thread.h"
#include "api.h"
#include "api/file.h"

extern "C" {
#include <lua.h>
}

#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <windows.h>

namespace fs = std::filesystem;

static Environment s_environment;

Environment &get_environment() {
	return s_environment;
}

static bool read_file_contents(const fs::path &path, std::string &out) {
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

static bool is_safe_relpath(const char *path) {
	if (!path || path[0] == '\0') return false;
	if (path[0] == '/' || path[0] == '\\') return false;
	if (path[1] == ':') return false;
	for (const char *p = path; *p; ++p) {
		if (*p == '\\') return false;
	}
	if (strstr(path, "..")) return false;
	return true;
}

static fs::path safe_resolve(const fs::path &modRoot, const char *relPath) {
	if (!is_safe_relpath(relPath)) return {};

	std::error_code ec;
	fs::path candidate = modRoot / relPath;
	fs::path canon = fs::canonical(candidate, ec);
	if (ec) return {};

	fs::path canonRoot = fs::canonical(modRoot, ec);
	if (ec) return {};

	std::string rootStr = canonRoot.generic_string();
	std::string pathStr = canon.generic_string();
	if (rootStr.back() != '/') rootStr += '/';
	if (pathStr.size() < rootStr.size()) return {};
	if (pathStr.compare(0, rootStr.size(), rootStr) != 0) return {};

	return canon;
}

// These seem to be safe:
//  lua_tostring, lua_pushnil, lua_pushstring, lua_pushcclosure,
//  lua_setglobal, lua_gettop, game_lua_settop, game_luaL_loadbuffer,
//  game_lua_pcall

// __env_loadchunk(modRoot, relPath) -> chunk | nil, err
static int env_loadchunk(lua_State *L) {
	const char *modRootStr = lua_tostring(L, 1);
	const char *relPath = lua_tostring(L, 2);

	if (!modRootStr || !relPath) {
		lua_pushnil(L);
		lua_pushstring(L, "loadchunk: missing arguments");
		return 2;
	}

	fs::path resolved = safe_resolve(fs::path(modRootStr), relPath);
	if (resolved.empty()) {
		lua_pushnil(L);
		lua_pushstring(L, "loadchunk: path not found or invalid");
		return 2;
	}

	std::string source;
	if (!read_file_contents(resolved, source)) {
		lua_pushnil(L);
		lua_pushstring(L, "loadchunk: could not read file");
		return 2;
	}

	if (!source.empty() && source[0] == '\033') {
		lua_pushnil(L);
		lua_pushstring(L, "loadchunk: bytecode loading is not permitted");
		return 2;
	}

	std::string chunkName = std::string("@") + relPath;
	int base = lua_gettop(L);
	int status = game_luaL_loadbuffer(L, source.c_str(), static_cast<int>(source.size()), chunkName.c_str());
	if (status != 0) {
		const char *err = lua_tostring(L, -1);
		std::string errCopy = err ? err : "load error";
		game_lua_settop(L, base);
		lua_pushnil(L);
		lua_pushstring(L, errCopy.c_str());
		return 2;
	}

	return 1;
}

// __env_loadstring(src, name) -> chunk | nil, err
// Safe wrapper for game_luaL_loadbuffer, used by sandboxed loadstring
static int env_loadstring(lua_State *L) {
	const char *src = lua_tostring(L, 1);
	const char *name = lua_tostring(L, 2);

	if (!src) {
		lua_pushnil(L);
		lua_pushstring(L, "bad argument #1 to loadstring (string expected)");
		return 2;
	}
	if (!name) name = "=(loadstring)";

	int base = lua_gettop(L);
	int status = game_luaL_loadbuffer(L, src, static_cast<int>(strlen(src)), name);
	if (status != 0) {
		const char *err = lua_tostring(L, -1);
		std::string errCopy = err ? err : "load error";
		game_lua_settop(L, base);
		lua_pushnil(L);
		lua_pushstring(L, errCopy.c_str());
		return 2;
	}

	return 1;
}

// __env_log(prefix, msg)
static int env_log(lua_State *L) {
	const char *prefix = lua_tostring(L, 1);
	const char *msg = lua_tostring(L, 2);
	std::string p = prefix ? prefix : "mod";
	std::string m = msg ? msg : "";
	spdlog::info("[{}] {}", p, m);
	return 0;
}

// __env_executor_log(msg)
static int env_executor_log(lua_State *L) {
	const char *msg = lua_tostring(L, 1);
	overlay_executor_log(msg ? msg : "");
	return 0;
}

static std::string query_game_version(lua_State *L) {
	int base = lua_gettop(L);
	const char *src = "return game.versionNumber()";
	int s = game_luaL_loadbuffer(L, src, static_cast<int>(strlen(src)), "=version_query");
	if (s != 0) {
		spdlog::debug("[env] game version query: loadbuffer failed");
		game_lua_settop(L, base);
		return {};
	}

	s = game_lua_pcall(L, 0, 1, 0);
	if (s != 0) {
		spdlog::debug("[env] game version query: pcall failed");
		game_lua_settop(L, base);
		return {};
	}

	const char *ver = lua_tostring(L, -1);
	std::string result = ver ? ver : "";
	game_lua_settop(L, base);
	return result;
}

static const char BOOTSTRAP_PRE[] = R"LUA(
local _loadchunk  = __env_loadchunk
local _loadstr    = __env_loadstring
local _logfn      = __env_log
local _shared     = __env_shared

local modId    = __MOD_ID
local modRoot  = __MOD_ROOT
local entry    = __MOD_ENTRY

local env = {}
setmetatable(env, { __index = _G })
env.shared = _shared
env._G     = env
env._MOD   = { id = modId, root = modRoot }

do
    local _env = env
    env.loadstring = function(src, name)
        if type(src) ~= 'string' then error('bad argument #1 to loadstring (string expected)') end
        if #src > 0 and src:byte(1) == 27 then error('bytecode loading is not permitted') end
        local chunk, err = _loadstr(src, name or '=(loadstring)')
        if not chunk then return nil, err end
        setfenv(chunk, _env)
        return chunk
    end
end

)LUA";

static const char BOOTSTRAP_POST[] = R"LUA(
do
    local _env = env
    local cache = {}
    env.require = function(name)
        if type(name) ~= 'string' then error('require: name must be a string') end
        if not name:match('^[%w_.]+$') or name:match('%.%.') then
            error("require: invalid module name '" .. name .. "'")
        end
        if name:sub(-4) == '.lua' then name = name:sub(1, -5) end
        if cache[name] ~= nil then return cache[name] end

        local path = name:gsub('%.', '/') .. '.lua'
        local chunk, err = _loadchunk(modRoot, path)
        if not chunk then error("require '" .. name .. "': " .. tostring(err)) end
        setfenv(chunk, _env)
        local result = chunk()
        cache[name] = (result ~= nil) and result or true
        return cache[name]
    end
end

env.print = function(...)
    local parts = {}
    for i = 1, select('#', ...) do
        parts[#parts + 1] = tostring(select(i, ...))
    end
    _logfn(modId, table.concat(parts, '\t'))
end

local chunk, err = _loadchunk(modRoot, entry)
if not chunk then error("failed to load entry '" .. entry .. "': " .. tostring(err)) end
setfenv(chunk, env)
return chunk
)LUA";

static const char EXEC_BOOTSTRAP_PRE[] = R"LUA(
local _loadstr = __env_loadstring
local _elog    = __env_executor_log

local code = __exec_code
__exec_code = nil

local chunk, err = _loadstr('return ' .. code, '=executor')
if not chunk then
    chunk, err = _loadstr(code, '=executor')
end
if not chunk then
    _elog('[error] ' .. tostring(err))
    return
end

local env = {}
setmetatable(env, { __index = _G })
env.print = function(...)
    local parts = {}
    for i = 1, select('#', ...) do
        parts[#parts + 1] = tostring(select(i, ...))
    end
    _elog(table.concat(parts, '\t'))
end

do
    local _env = env
    env.loadstring = function(src, name)
        if type(src) ~= 'string' then error('bad argument #1 to loadstring (string expected)') end
        if #src > 0 and src:byte(1) == 27 then error('bytecode loading is not permitted') end
        local c2, e2 = _loadstr(src, name or '=(loadstring)')
        if not c2 then return nil, e2 end
        setfenv(c2, _env)
        return c2
    end
end
)LUA";

static const char EXEC_BOOTSTRAP_POST[] = R"LUA(
setfenv(chunk, env)
return chunk
)LUA";

static void set_global_cfunc(lua_State *L, const char *name, lua_CFunction fn) {
	lua_pushcclosure(L, fn, 0);
	lua_setglobal(L, name);
}

static void set_global_string(lua_State *L, const char *name, const char *value) {
	lua_pushstring(L, value);
	lua_setglobal(L, name);
}

static void clear_global(lua_State *L, const char *name) {
	lua_pushnil(L);
	lua_setglobal(L, name);
}

Sandbox &Environment::sandbox() {
	return m_sandbox;
}

void Environment::set_mod_context(const std::string &id, const std::string &root) {
	m_mod_id = id;
	m_mod_root = root;
}

void Environment::clear_mod_context() {
	m_mod_id.clear();
	m_mod_root.clear();
}

const std::string &Environment::mod_id() const {
	return m_mod_id;
}

const std::string &Environment::mod_root() const {
	return m_mod_root;
}

void Environment::register_apis(lua_State *L, const std::string &gameDirStr, const ModLoader *loader) {
	set_global_cfunc(L, "__env_loadchunk", env_loadchunk);
	set_global_cfunc(L, "__env_loadstring", env_loadstring);
	set_global_cfunc(L, "__env_log", env_log);
	set_global_cfunc(L, "__env_executor_log", env_executor_log);

	api_register_all(L, gameDirStr, loader);

	spdlog::debug("[env] APIs registered (gameDir='{}')", gameDirStr);
}

void Environment::load_mods(lua_State *L, const ModLoader *loader, const std::string &activeVersion) {
	const auto &mods = loader->getAllMods();

	spdlog::info("[env] loading {} mod(s) (game version '{}')", mods.size(), activeVersion);

	{
		const char *src = "__env_shared = {}";
		int base = lua_gettop(L);
		int s = game_luaL_loadbuffer(L, src, static_cast<int>(strlen(src)), "=env_shared");
		if (s == 0) game_lua_pcall(L, 0, 0, 0);
		game_lua_settop(L, base);
	}

	set_global_cfunc(L, "__env_loadchunk", env_loadchunk);

	std::string sandboxCode = m_sandbox.generate_lua("env");

	for (size_t i = 0; i < mods.size(); i++) {
		const Manifest &manifest = mods[i]->getManifest();
		const fs::path &modPath = mods[i]->getPath();
		fs::path entryPath = modPath / manifest.entry;

		if (!fs::exists(entryPath)) {
			spdlog::debug("[env] '{}': entry file '{}' not found, skipping", manifest.id, manifest.entry);
			continue;
		}

		if (manifest.error_on_game_update && manifest.game_version != activeVersion) {
			std::string msg = "Mod '" + manifest.name + "' (" + manifest.id + ") was built for game version " +
							  manifest.game_version + " but the current game version is " + activeVersion +
							  ".\n\nLoading it may cause errors or unexpected behavior."
							  "\n\nProceed loading this mod?";

			int choice = MessageBoxA(nullptr, msg.c_str(), "Jeode - Version Mismatch",
									 MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
			if (choice != IDYES) {
				spdlog::info("[env] '{}' skipped by user (version mismatch: mod={}, game={})", manifest.id,
							 manifest.game_version, activeVersion);
				overlay_executor_log("[warn] [" + manifest.id + "] skipped (version mismatch)");
				continue;
			}
		}

		std::error_code ec;
		fs::path canonRoot = fs::canonical(modPath, ec);
		if (ec) {
			spdlog::warn("[env] '{}': could not resolve mod path, skipping", manifest.id);
			continue;
		}
		std::string rootStr = canonRoot.generic_string();

		set_mod_context(manifest.id, "mods/" + modPath.filename().string());
		file_api_set_mod_root(m_mod_root.c_str());

		set_global_string(L, "__MOD_ID", manifest.id.c_str());
		set_global_string(L, "__MOD_ROOT", rootStr.c_str());
		set_global_string(L, "__MOD_ENTRY", manifest.entry.c_str());

		std::string bootstrap;
		bootstrap.reserve(sizeof(BOOTSTRAP_PRE) + sandboxCode.size() + sizeof(BOOTSTRAP_POST) + 64);
		bootstrap += BOOTSTRAP_PRE;
		bootstrap += sandboxCode;
		bootstrap += BOOTSTRAP_POST;

		int base = lua_gettop(L);

		int s = game_luaL_loadbuffer(L, bootstrap.c_str(), static_cast<int>(bootstrap.size()), "=sandbox=loader");
		if (s != 0) {
			const char *err = lua_tostring(L, -1);
			std::string errmsg = err ? err : "(unknown)";
			spdlog::error("[env] '{}': bootstrap load error: {}", manifest.id, errmsg);
			overlay_executor_log("[error] [" + manifest.id + "] " + errmsg);
			game_lua_settop(L, base);
			continue;
		}

		s = game_lua_pcall(L, 0, 1, 0);
		if (s != 0) {
			const char *err = lua_tostring(L, -1);
			std::string errmsg = err ? err : "(unknown error)";
			spdlog::error("[env] '{}': bootstrap error: {}", manifest.id, errmsg);
			overlay_executor_log("[error] [" + manifest.id + "] " + errmsg);
			game_lua_settop(L, base);
			continue;
		}

		if (!lua_isfunction(L, -1)) {
			spdlog::error("[env] '{}': bootstrap did not return a function", manifest.id);
			game_lua_settop(L, base);
			continue;
		}

		lua_State *co = (lua_State *)game_lua_newthread(L);
		lua_pushvalue(L, -2);
		lua_xmove(L, co, 1);

		int r = game_lua_resume(L, co, 0);
		if (r < 0) {
			const char *err = lua_tostring(L, -1);
			std::string errmsg = err ? err : "(unknown error)";
			spdlog::error("[env] '{}': runtime error: {}", manifest.id, errmsg);
			overlay_executor_log("[error] [" + manifest.id + "] " + errmsg);
			game_lua_settop(L, base);
			continue;
		}

		game_lua_settop(L, base);
		spdlog::info("[env] '{}' loaded successfully", manifest.id);
	}

	clear_global(L, "__MOD_ID");
	clear_global(L, "__MOD_ROOT");
	clear_global(L, "__MOD_ENTRY");
	clear_global(L, "__env_shared");
	spdlog::debug("[env] mod loading complete, globals cleaned up");
}

void Environment::init(lua_State *L, const ModLoader *loader, const fs::path &gameDir, const JeodeConfig &config) {
	if (!loader) return;

	m_sandbox.set_allow_unsafe(config.allow_unsafe_functions);
	if (config.allow_unsafe_functions) spdlog::warn("[env] unsafe functions are enabled");

	std::string gameVersion = query_game_version(L);
	std::string buildVersion = version::game_version();

	if (!gameVersion.empty() && gameVersion != buildVersion)
		spdlog::warn("[env] game version '{}' differs from build version '{}'", gameVersion, buildVersion);

	std::error_code ec;
	std::string gameDirStr = fs::canonical(gameDir, ec).generic_string();

	register_apis(L, gameDirStr, loader);

	std::string activeVersion = gameVersion.empty() ? buildVersion : gameVersion;
	spdlog::info("[env] game version: '{}', {} mod(s) to load", activeVersion, loader->getAllMods().size());

	if (!loader->getAllMods().empty()) {
		load_mods(L, loader, activeVersion);
	}

	spdlog::info("[env] environment initialization complete");
}

static std::string stack_value_to_string(lua_State *L, int idx) {
	int tp = lua_type(L, idx);
	switch (tp) {
	case LUA_TSTRING:
	case LUA_TNUMBER: {
		const char *s = lua_tostring(L, idx);
		return s ? s : "";
	}
	case LUA_TBOOLEAN:
		return lua_toboolean(L, idx) ? "true" : "false";
	case LUA_TNIL:
		return "nil";
	default:
		return lua_typename(L, tp);
	}
}

void Environment::execute(const std::string &code) {
	std::string sandboxCode = m_sandbox.generate_lua("env");

	lua_thread_queue([this, code, sandboxCode](lua_State *L) {
		if (!L) {
			overlay_executor_log("[error] lua state not available");
			return;
		}

		set_mod_context("executor", "mods/executor");
		file_api_set_mod_root("mods/executor");

		int base = lua_gettop(L);

		lua_pushstring(L, code.c_str());
		lua_setglobal(L, "__exec_code");

		std::string bootstrap;
		bootstrap.reserve(sizeof(EXEC_BOOTSTRAP_PRE) + sandboxCode.size() + sizeof(EXEC_BOOTSTRAP_POST) + 64);
		bootstrap += EXEC_BOOTSTRAP_PRE;
		bootstrap += sandboxCode;
		bootstrap += EXEC_BOOTSTRAP_POST;

		int s = game_luaL_loadbuffer(L, bootstrap.c_str(), static_cast<int>(bootstrap.size()), "=executor");
		if (s != 0) {
			const char *err = lua_tostring(L, -1);
			overlay_executor_log(std::string("[error] ") + (err ? err : "executor load error"));
			game_lua_settop(L, base);
			clear_mod_context();
			file_api_clear_mod_root();
			return;
		}

		s = game_lua_pcall(L, 0, 1, 0);
		if (s != 0) {
			const char *err = lua_tostring(L, -1);
			overlay_executor_log(std::string("[error] ") + (err ? err : "executor runtime error"));
			game_lua_settop(L, base);
			clear_mod_context();
			file_api_clear_mod_root();
			return;
		}

		if (!lua_isfunction(L, -1)) {
			game_lua_settop(L, base);
			clear_mod_context();
			file_api_clear_mod_root();
			return;
		}

		lua_State *co = (lua_State *)game_lua_newthread(L);
		lua_pushvalue(L, -2);
		lua_xmove(L, co, 1);

		int nresults = game_lua_resume(L, co, 0);
		if (nresults < 0) {
			const char *err = lua_tostring(L, -1);
			overlay_executor_log(std::string("[error] ") + (err ? err : "executor runtime error"));
			game_lua_settop(L, base);
			clear_mod_context();
			file_api_clear_mod_root();
			return;
		}

		if (lua_status(co) != LUA_YIELD && nresults > 0) {
			int rbase = lua_gettop(L) - nresults + 1;
			std::string parts;
			for (int i = 0; i < nresults; i++) {
				if (i > 0) parts += "\t";
				parts += stack_value_to_string(L, rbase + i);
			}
			overlay_executor_log(parts);
		}

		game_lua_settop(L, base);
		clear_mod_context();
		file_api_clear_mod_root();
	});
}
