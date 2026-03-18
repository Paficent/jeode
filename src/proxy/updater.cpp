#include <windows.h>

#include <bcrypt.h>
#include <wininet.h>

#include "proxy/updater.h"
#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

static constexpr int64_t CHECK_INTERVAL_SECONDS = 3600;
static constexpr const char *GITHUB_API_LATEST = "https://api.github.com/repos/Paficent/jeode/releases/latest";
static constexpr const char *HTTP_USER_AGENT = "jeode-updater/1.0";

struct AssetInfo {
	std::string name;
	std::string sha256;
	std::string download_url;
};

struct InetHandle {
	HINTERNET h = nullptr;
	~InetHandle() {
		if (h) InternetCloseHandle(h);
	}
	operator HINTERNET() const { return h; }
	explicit operator bool() const { return h != nullptr; }
};

static int64_t current_timestamp() {
	auto now = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

static std::string bytes_to_hex(const uint8_t *data, size_t len) {
	static const char digits[] = "0123456789abcdef";
	std::string result;
	result.reserve(len * 2);
	for (size_t i = 0; i < len; ++i) {
		result += digits[data[i] >> 4];
		result += digits[data[i] & 0x0f];
	}
	return result;
}

static std::string sha256_of(const uint8_t *data, size_t len) {
	BCRYPT_ALG_HANDLE alg = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	uint8_t digest[32] = {};

	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};

	if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
		BCryptCloseAlgorithmProvider(alg, 0);
		return {};
	}

	BCryptHashData(hash, const_cast<uint8_t *>(data), static_cast<ULONG>(len), 0);
	BCryptFinishHash(hash, digest, sizeof(digest), 0);
	BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(alg, 0);

	return bytes_to_hex(digest, sizeof(digest));
}

static std::string sha256_file(const fs::path &path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) return {};

	std::vector<uint8_t> contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return sha256_of(contents.data(), contents.size());
}

static std::string extract_sha256(const std::string &digest_field) {
	constexpr const char *prefix = "sha256:";
	constexpr size_t prefix_len = 7;
	if (digest_field.size() > prefix_len && digest_field.compare(0, prefix_len, prefix) == 0)
		return digest_field.substr(prefix_len);
	return digest_field;
}

static std::string http_get(const char *url) {
	InetHandle session;
	session.h = InternetOpenA(HTTP_USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
	if (!session) return {};

	const char *headers = "Accept: application/vnd.github+json\r\n";
	InetHandle request;
	request.h = InternetOpenUrlA(session, url, headers, static_cast<DWORD>(-1),
								 INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
	if (!request) return {};

	std::string response;
	char buffer[8192];
	DWORD bytes_read = 0;
	while (InternetReadFile(request, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0)
		response.append(buffer, bytes_read);

	return response;
}

static bool download_to_file(const std::string &url, const fs::path &dest) {
	InetHandle session;
	session.h = InternetOpenA(HTTP_USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
	if (!session) return false;

	InetHandle request;
	request.h = InternetOpenUrlA(session, url.c_str(), nullptr, 0,
								 INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
	if (!request) return false;

	std::ofstream file(dest, std::ios::binary);
	if (!file) return false;

	char buffer[8192];
	DWORD bytes_read = 0;
	while (InternetReadFile(request, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0)
		file.write(buffer, bytes_read);

	return file.good();
}

static void cleanup_old_files(const fs::path &gameDir) {
	std::error_code ec;
	fs::remove(gameDir / "winhttp.dll.old", ec);
	fs::remove(gameDir / "jeode" / "libjeode.dll.old", ec);
}

static bool stage_asset(const AssetInfo &asset, const fs::path &local_path) {
	fs::path staged_path = fs::path(local_path).concat(".new");

	if (!download_to_file(asset.download_url, staged_path)) return false;

	std::string downloaded_hash = sha256_file(staged_path);
	if (downloaded_hash.empty() || downloaded_hash != asset.sha256) {
		std::error_code ec;
		fs::remove(staged_path, ec);
		return false;
	}

	return true;
}

static bool apply_staged(const fs::path &local_path) {
	fs::path staged_path = fs::path(local_path).concat(".new");
	fs::path backup_path = fs::path(local_path).concat(".old");

	std::error_code ec;
	fs::rename(local_path, backup_path, ec);
	if (ec) return false;

	fs::rename(staged_path, local_path, ec);
	if (ec) {
		fs::rename(backup_path, local_path, ec);
		return false;
	}

	return true;
}

static std::map<std::string, AssetInfo> parse_release_assets(const json &release) {
	std::map<std::string, AssetInfo> assets;

	if (!release.contains("assets") || !release["assets"].is_array()) return assets;

	for (const auto &entry : release["assets"]) {
		if (!entry.contains("name") || !entry["name"].is_string()) continue;
		if (!entry.contains("digest") || !entry["digest"].is_string()) continue;
		if (!entry.contains("browser_download_url") || !entry["browser_download_url"].is_string()) continue;

		AssetInfo info;
		info.name = entry["name"].get<std::string>();
		info.sha256 = extract_sha256(entry["digest"].get<std::string>());
		info.download_url = entry["browser_download_url"].get<std::string>();
		assets[info.name] = std::move(info);
	}

	return assets;
}

struct UpdateTarget {
	std::string asset_name;
	fs::path local_path;
};

static bool needs_update(const AssetInfo &asset, const fs::path &local_path) {
	if (!fs::exists(local_path)) return false;
	std::string local_hash = sha256_file(local_path);
	return !local_hash.empty() && local_hash != asset.sha256;
}

void updater_run(JeodeConfig &cfg, const fs::path &gameDir) {
	cleanup_old_files(gameDir);

	if ((current_timestamp() - cfg.last_update_check) < CHECK_INTERVAL_SECONDS) return;

	std::string response = http_get(GITHUB_API_LATEST);
	if (response.empty()) return;

	json release;
	try {
		release = json::parse(response);
	} catch (...) {
		return;
	}

	auto remote_assets = parse_release_assets(release);
	if (remote_assets.empty()) return;

	cfg.last_update_check = current_timestamp();
	config_save(cfg, gameDir / "jeode");

	std::vector<UpdateTarget> pending;

	auto check_target = [&](const char *asset_name, const fs::path &local_path) {
		auto it = remote_assets.find(asset_name);
		if (it != remote_assets.end() && needs_update(it->second, local_path))
			pending.push_back({asset_name, local_path});
	};

	check_target("winhttp.dll", gameDir / "winhttp.dll");
	check_target("libjeode.dll", gameDir / "jeode" / "libjeode.dll");

	if (pending.empty()) return;

	std::string tag = release.value("tag_name", "unknown");
	std::string prompt = "A new Jeode update is available (v" + tag + ").\n\nUpdate now?";
	int choice = MessageBoxA(nullptr, prompt.c_str(), "Jeode Updater", MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST);
	if (choice != IDYES) return;

	bool any_applied = false;
	for (const auto &target : pending) {
		const auto &asset = remote_assets.at(target.asset_name);
		if (stage_asset(asset, target.local_path) && apply_staged(target.local_path)) any_applied = true;
	}

	if (!any_applied) return;

	MessageBoxA(nullptr, "Update installed successfully.\nMy Singing Monsters will now close.", "Jeode Updater",
				MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	ExitProcess(0);
}
