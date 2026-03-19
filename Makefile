# TODO: switch to cmake
CC  := i686-w64-mingw32-gcc
CXX := i686-w64-mingw32-g++

MH_DIR       := include/minhook/src
LUA_DIR      := include/lua-5.1.5/src
IMGUI_DIR    := include/imgui
TEXTEDIT_DIR := include/ImGuiColorTextEdit
BUILD_DIR    := build

PROXY_TARGET  := $(BUILD_DIR)/winhttp.dll
LOADER_TARGET := $(BUILD_DIR)/jeode/libjeode.dll

JEODE_VERSION ?= dev

CFLAGS   := -Wall -Wextra -Os -std=c11 -ffunction-sections -fdata-sections \
            -DWIN32_LEAN_AND_MEAN -DNOMINMAX
CXXFLAGS := -Wall -Wextra -Os -std=c++17 -ffunction-sections -fdata-sections -fno-rtti \
            -DWIN32_LEAN_AND_MEAN -DNOMINMAX \
            -DJEODE_VERSION=\"$(JEODE_VERSION)\"
IMGUI_CXXFLAGS := -Os -std=c++17 -ffunction-sections -fdata-sections -fno-rtti \
            -DWIN32_LEAN_AND_MEAN -DNOMINMAX -w
LDFLAGS  := -shared -static -static-libgcc -static-libstdc++ \
            -Wl,--gc-sections -Wl,--strip-all -Wl,--enable-stdcall-fixup

PROXY_LDFLAGS  := $(LDFLAGS)
PROXY_LIBS     := -lwinhttp -lwininet -lbcrypt -lversion
LOADER_LDFLAGS := $(LDFLAGS)
LOADER_LIBS    := -ldwmapi -lgdi32 -limm32

INCLUDES := \
    -Isrc \
    -Iinclude/minhook/include \
    -Iinclude/minhook/src \
    -I$(LUA_DIR) \
    -I$(IMGUI_DIR) \
    -I$(IMGUI_DIR)/backends \
    -I$(TEXTEDIT_DIR) \
    -isystem include

# winhttp.dll
PROXY_CPP_SRCS := \
    src/proxy/proxy_main.cpp \
    src/proxy/proxy.cpp \
    src/proxy/updater.cpp \
    src/core/config.cpp \
    src/core/keybind.cpp

PROXY_ASM_SRCS := src/proxy/stubs_proxy.S

PROXY_CPP_OBJS := $(PROXY_CPP_SRCS:%.cpp=$(BUILD_DIR)/%.o)
PROXY_ASM_OBJS := $(PROXY_ASM_SRCS:%.S=$(BUILD_DIR)/%.o)
PROXY_OBJS     := $(PROXY_CPP_OBJS) $(PROXY_ASM_OBJS)

# libjeode.dll
LOADER_CPP_SRCS := \
    src/core/main.cpp \
    src/core/config.cpp \
    src/core/log.cpp \
    src/core/keybind.cpp \
    src/env/environment.cpp \
    src/env/sandbox.cpp \
    src/env/api.cpp \
    src/env/api/console.cpp \
    src/env/api/file.cpp \
    src/env/api/mod.cpp \
    src/lua/game_lua.cpp \
    src/lua/thread.cpp \
    src/core/overlay.cpp \
    src/hooks/hook_manager.cpp \
    src/hooks/file_hook.cpp \
    src/hooks/ssl_hook.cpp \
    src/hooks/scheduler_hook.cpp \
    src/hooks/egl_hook.cpp \
    src/loader/schema.cpp \
    src/loader/mod.cpp \
    src/loader/mod_loader.cpp \
    src/loader/native_mod.cpp

IMGUI_CPP_SRCS := \
    $(IMGUI_DIR)/imgui.cpp \
    $(IMGUI_DIR)/imgui_draw.cpp \
    $(IMGUI_DIR)/imgui_tables.cpp \
    $(IMGUI_DIR)/imgui_widgets.cpp \
    $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp \
    $(IMGUI_DIR)/backends/imgui_impl_win32.cpp \
    $(TEXTEDIT_DIR)/TextEditor.cpp

MH_SRCS := \
    $(MH_DIR)/buffer.c \
    $(MH_DIR)/hook.c \
    $(MH_DIR)/trampoline.c \
    $(MH_DIR)/hde/hde32.c

LUA_SRCS := \
    $(LUA_DIR)/lapi.c \
    $(LUA_DIR)/lauxlib.c \
    $(LUA_DIR)/lbaselib.c \
    $(LUA_DIR)/lcode.c \
    $(LUA_DIR)/ldblib.c \
    $(LUA_DIR)/ldebug.c \
    $(LUA_DIR)/ldo.c \
    $(LUA_DIR)/ldump.c \
    $(LUA_DIR)/lfunc.c \
    $(LUA_DIR)/lgc.c \
    $(LUA_DIR)/linit.c \
    $(LUA_DIR)/liolib.c \
    $(LUA_DIR)/llex.c \
    $(LUA_DIR)/lmathlib.c \
    $(LUA_DIR)/lmem.c \
    $(LUA_DIR)/loadlib.c \
    $(LUA_DIR)/lobject.c \
    $(LUA_DIR)/lopcodes.c \
    $(LUA_DIR)/loslib.c \
    $(LUA_DIR)/lparser.c \
    $(LUA_DIR)/lstate.c \
    $(LUA_DIR)/lstring.c \
    $(LUA_DIR)/lstrlib.c \
    $(LUA_DIR)/ltable.c \
    $(LUA_DIR)/ltablib.c \
    $(LUA_DIR)/ltm.c \
    $(LUA_DIR)/lundump.c \
    $(LUA_DIR)/lvm.c \
    $(LUA_DIR)/lzio.c \
    $(LUA_DIR)/print.c

LOADER_ASM_SRCS := src/lua/stubs_lua.S

LOADER_CPP_OBJS := $(LOADER_CPP_SRCS:%.cpp=$(BUILD_DIR)/%.o)
IMGUI_CPP_OBJS  := $(IMGUI_CPP_SRCS:%.cpp=$(BUILD_DIR)/%.o)
MH_OBJS         := $(MH_SRCS:%.c=$(BUILD_DIR)/%.o)
LUA_OBJS        := $(LUA_SRCS:$(LUA_DIR)/%.c=$(BUILD_DIR)/lua/%.o)
LOADER_ASM_OBJS := $(LOADER_ASM_SRCS:%.S=$(BUILD_DIR)/%.o)
LOADER_OBJS     := $(LOADER_CPP_OBJS) $(IMGUI_CPP_OBJS) $(MH_OBJS) $(LUA_OBJS) $(LOADER_ASM_OBJS)

ALL_OBJS := $(PROXY_OBJS) $(LOADER_OBJS)
DEPS     := $(ALL_OBJS:.o=.d)

# Targets
all: deps $(PROXY_TARGET) $(LOADER_TARGET)

deps:
	@if [ ! -d include/minhook ] || [ ! -d include/nlohmann ] || \
	    [ ! -d include/spdlog ] || [ ! -d include/lua-5.1.5 ] || \
	    [ ! -d include/imgui ] || [ ! -d include/ImGuiColorTextEdit ]; then \
		bash scripts/dependencies.sh; \
	fi

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/src/{core,proxy,hooks,lua,loader,overlay,env,env/api}
	mkdir -p $(BUILD_DIR)/$(MH_DIR)/hde
	mkdir -p $(BUILD_DIR)/$(IMGUI_DIR)/backends
	mkdir -p $(BUILD_DIR)/$(TEXTEDIT_DIR)
	mkdir -p $(BUILD_DIR)/lua
	mkdir -p $(BUILD_DIR)/jeode

# Project C++ sources
$(BUILD_DIR)/src/%.o: src/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# ImGui C++ sources (relaxed warnings)
$(BUILD_DIR)/$(IMGUI_DIR)/%.o: $(IMGUI_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(IMGUI_CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# ImGuiColorTextEdit (relaxed warnings)
$(BUILD_DIR)/$(TEXTEDIT_DIR)/%.o: $(TEXTEDIT_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(IMGUI_CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/$(MH_DIR)/%.o: $(MH_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/lua/%.o: $(LUA_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@

$(PROXY_TARGET): $(PROXY_OBJS) src/winhttp.def
	$(CXX) $(PROXY_LDFLAGS) -o $@ $(PROXY_OBJS) src/winhttp.def $(PROXY_LIBS)
	@echo "Built: $(PROXY_TARGET) ($(JEODE_VERSION))"

$(LOADER_TARGET): $(LOADER_OBJS) src/libjeode.def
	$(CXX) $(LOADER_LDFLAGS) -o $@ $(LOADER_OBJS) src/libjeode.def $(LOADER_LIBS)
	@echo "Built: $(LOADER_TARGET) ($(JEODE_VERSION))"

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf $(BUILD_DIR)
	rm -rf include

-include $(DEPS)

.PHONY: all clean distclean dist deps
