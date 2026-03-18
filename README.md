# Jeode

[![GitHub](https://img.shields.io/badge/GitHub-Source-181717?logo=github)](https://github.com/Paficent/jeode)
[![Codeberg](https://img.shields.io/badge/Codeberg-Mirror-2185D0?logo=codeberg)](https://codeberg.org/Paficent/jeode)
[![Discord](https://img.shields.io/discord/1477174015926997155?label=Discord&logo=discord&color=5865F2)](https://discord.gg/FSHq6DaRnX)
[![License](https://img.shields.io/github/license/Paficent/jeode)](https://github.com/Paficent/jeode/blob/main/LICENSE)

Jeode is a mod loader and framework for the PC version of [My Singing Monsters](https://store.steampowered.com/app/1419170/My_Singing_Monsters/), designed to make loading mods simple and development more powerful.

## Installation

### Installer (recommended)

Download `jeode-installer.exe` from the [latest release](https://github.com/Paficent/jeode/releases/latest) and run it. The installer will attempt to locate the My Singing Monsters directory. If it can't find it, you will have to manually locate the folder containing `MySingingMonsters.exe`.

### Manual Installation

Download `winhttp.dll` and `libjeode.dll` from the [latest release](https://github.com/Paficent/jeode/releases/latest) and place them in the MSM folder like so:

```
My Singing Monsters/
├── MySingingMonsters.exe
├── winhttp.dll
└── jeode/
    └── libjeode.dll
```

Note: We use a proxy `winhttp.dll` to have the game load Jeode automatically, which means no clunky DLL injector required.

### Wine / Proton

On Linux or macOS, the game won't load a local `winhttp.dll` unless you tell Wine to prefer it over the builtin version.

1. Run the installer in your wine prefix or use the manual method.
2. Open a terminal and run `winecfg` (or use `protontricks 1419170 winecfg` for Steam/Proton)
3. Go to the **Libraries** tab.
4. In the **New override for library** field, type `winhttp` and click **Add**.
5. Select `winhttp` in the list, click **Edit**, and set it to **Native, Builtin**.
6. Click **OK** and launch MSM.

## Mods

Place mods in the `mods/` folder inside your game directory. Each mod is a subfolder with a `manifest.json`, an optional `data` folder, and an optional entry (typically `init.lua`):

```
My Singing Monsters/
└── mods/
    └── my-mod/
        ├── data/
        ├── manifest.json
        └── init.lua
```

See the [examples](examples/) folder for reference.

## Support

For help, join our [Discord](https://discord.gg/FSHq6DaRnX) or [file an issue](https://github.com/Paficent/jeode/issues).
