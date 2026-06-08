---
icon: lucide/package-open
---

# Installation

There are a few ways to get Jeode onto your game:

!!! warning "Antivirus false positives"
    Some antiviruses (notably Windows Defender, Avast, and AVG) wrongly flag Jeode as a virus. If yours does, add `jeode-installer.exe`, `libjeode.dll`, or your My Singing Monsters folder as an exclusion.

---

## Installer

The simplest option. Download `jeode-installer.exe` from the [latest release](https://github.com/Paficent/jeode/releases/latest) and run it. It tries to find your My Singing Monsters folder on its own, if it can't, just point it at the folder containing `MySingingMonsters.exe`.

---

## Cantus

[Cantus](https://github.com/Paficent/cantus/releases/latest) is a mod manager with a setup wizard that installs Jeode for you as part of getting started.

---

## Manual

Prefer to do it yourself? Download `winhttp.dll` and `libjeode.dll` from the [latest release](https://github.com/Paficent/jeode/releases/latest) and drop them in like so:

```text
My Singing Monsters/
├── MySingingMonsters.exe
├── winhttp.dll
└── jeode/
    └── libjeode.dll
```

The `winhttp.dll` is a proxy that gets the game to load Jeode automatically on launch. It also holds the code for Jeode's auto updater.

---

## Wine / Proton

On Linux or macOS the game won't load a local `winhttp.dll` until you tell Wine to prefer it over the built in one.

=== "Steam"

    1. Right click **My Singing Monsters** in your library and pick **Properties**.
    2. Paste this into the **Launch Options** box:

        ```text
        WINEDLLOVERRIDES="winhttp=n,b" %command%
        ```

    3. Close the window and launch the game.

=== "Other"

    1. Install Jeode with the installer or the manual steps above.
    2. Run `winecfg` (or `protontricks 1419170 winecfg` for Proton).
    3. Open the **Libraries** tab.
    4. Type `winhttp` into **New override for library** and click **Add**.
    5. Select it, click **Edit**, and set it to **Native, Builtin**.
    6. Click **OK** and launch the game.

---

## Did it work?

Launch the game. If Jeode is running, it creates a `jeode` and `mods` folder inside your game directory and starts loading anything in `mods`.

If you're still experiencing issues and have added Jeode to your antivirus exclusions, we're happy to provide you with [Support](../community/).
