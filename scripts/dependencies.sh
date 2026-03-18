#!/bin/bash
set -euo pipefail

INCLUDE_DIR="./include"

MINHOOK_TAG="v1.3.4"
NLOHMANN_TAG="v3.12.0"
SPDLOG_TAG="v1.17.0"
IMGUI_TAG="v1.92.6"
LUA_VER="5.1.5"

download_github() {
  local repo="$1" tag="$2" dest="$3" strip="${4:-1}"
  local url="https://github.com/${repo}/archive/refs/tags/${tag}.tar.gz"

  if [ -d "${INCLUDE_DIR}/${dest}" ]; then
    echo "  ${dest}: already exists, skipping"
    return
  fi

  echo "  ${dest}: downloading ${repo}@${tag}"
  mkdir -p "${INCLUDE_DIR}/${dest}"
  curl -sL "$url" | tar xz --strip-components="$strip" -C "${INCLUDE_DIR}/${dest}"
}

download_github_branch() {
  local repo="$1" branch="$2" dest="$3" strip="${4:-1}"
  local url="https://github.com/${repo}/archive/refs/heads/${branch}.tar.gz"

  if [ -d "${INCLUDE_DIR}/${dest}" ]; then
    echo "  ${dest}: already exists, skipping"
    return
  fi

  echo "  ${dest}: downloading ${repo}@${branch}"
  mkdir -p "${INCLUDE_DIR}/${dest}"
  curl -sL "$url" | tar xz --strip-components="$strip" -C "${INCLUDE_DIR}/${dest}"
}

download_lua() {
  local dest="lua-${LUA_VER}"
  local url="https://www.lua.org/ftp/lua-${LUA_VER}.tar.gz"

  if [ -d "${INCLUDE_DIR}/${dest}" ]; then
    echo "  ${dest}: already exists, skipping"
    return
  fi

  echo "  ${dest}: downloading lua ${LUA_VER}"
  mkdir -p "${INCLUDE_DIR}/${dest}"
  curl -sL "$url" | tar xz --strip-components=1 -C "${INCLUDE_DIR}/${dest}"
}

echo "Fetching dependencies..."
download_github "TsudaKageyu/minhook" "$MINHOOK_TAG" "minhook"
download_github "ocornut/imgui" "$IMGUI_TAG" "imgui"

# Patch imgui's GL loader for ANGLE (libGLESv2.dll + eglGetProcAddress)
# I'm sure there's a better more future proof method of doing this but this is the janky solution I came up with
PATCH_FILE="./scripts/imgui_angle_loader.patch"
LOADER_FILE="${INCLUDE_DIR}/imgui/backends/imgui_impl_opengl3_loader.h"
if [ -f "$PATCH_FILE" ] && [ -f "$LOADER_FILE" ]; then
  if grep -q "opengl32.dll" "$LOADER_FILE"; then
    echo "  imgui: applying ANGLE loader patch"
    patch -p1 < "$PATCH_FILE"
  fi
fi

download_github_branch "BalazsJako/ImGuiColorTextEdit" "master" "ImGuiColorTextEdit"

download_lua

# nlohmann/json
if [ -d "${INCLUDE_DIR}/nlohmann" ]; then
  echo "  nlohmann: already exists, skipping"
else
  echo "  nlohmann: downloading nlohmann/json@${NLOHMANN_TAG}"
  mkdir -p "${INCLUDE_DIR}/nlohmann"
  curl -sL "https://github.com/nlohmann/json/releases/download/${NLOHMANN_TAG}/json.hpp" \
    -o "${INCLUDE_DIR}/nlohmann/json.hpp"
  curl -sL "https://github.com/nlohmann/json/releases/download/${NLOHMANN_TAG}/json_fwd.hpp" \
    -o "${INCLUDE_DIR}/nlohmann/json_fwd.hpp"
fi

# spdlog
if [ -d "${INCLUDE_DIR}/spdlog" ]; then
  echo "  spdlog: already exists, skipping"
else
  echo "  spdlog: downloading gabime/spdlog@${SPDLOG_TAG}"
  tmp=$(mktemp -d)
  curl -sL "https://github.com/gabime/spdlog/archive/refs/tags/${SPDLOG_TAG}.tar.gz" \
    | tar xz --strip-components=1 -C "$tmp"
  mv "$tmp/include/spdlog" "${INCLUDE_DIR}/spdlog"
  rm -rf "$tmp"
fi

echo "Done"
