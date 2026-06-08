---
icon: lucide/puzzle
---

# jeode

The `jeode` table is for mod introspection, plus a couple of advanced tools.

---

## jeode.getVersion

```luau
jeode.getVersion(): string
```

Returns the Jeode version as a string, like `"5.4.2.0"`.

---

## jeode.getModId

```luau
jeode.getModId(): string
```

Returns the id of the mod that's currently running, the same value stored in your mods `manifest.json`.

---

## jeode.getModPath

```luau
jeode.getModPath(): string
```

Returns the path to your mods directory relative to MSM's directory, like
`mods/my-mod`. Useful in conjunction with the [fs](fs.md) api:

```lua
local savePath = jeode.getModPath() .. "/save.txt"
fs.write(savePath, "hi")
```

---

## jeode.getMods

```luau
jeode.getMods() -> {string}
```

Returns a table containing the ids of every loaded mod.

---

## jeode.getModInfo

```luau
jeode.getModInfo(id: string) -> {
    id: string,
    name: string,
    author: string,
    version: string,
    gameVersion: string, -- "X.X.X"
    root: string, -- Relative path to the mods folder
} | nil
```

Looks up details about a mod by its id, returns a table of info or `nil` if the mod is not found.

```lua
local info = jeode.getModInfo("another-mod")
if info then
    print(info.name .. " by " .. info.author)
end
```

---

## jeode.registerGlobal

```luau
jeode.registerGlobal(name: string, value: any) -> nil
```

Bypasses the sandbox and registers a global that every mod, and the game itself, can see.

Most of the time you don't need this, and it only should be used to pass data to patched XML/Lua files that need to access information from your mod.

```lua
local private = "hidden"
jeode.registerGlobal("globalLibrary", {
    broadcasted = private,
    notify = function(message)
        game.displayNotification(message)
    end,
})
```

!!! tip "Prefer `shared` for inter-mod communication"

---

## jeode.runAsMod

```luau
jeode.runAsMod(id: string, fn: (...any) -> any) -> nil
```

Runs `fn` in a spoofed environment so that calls which depend on "the current mod" (like [`jeode.getModPath`](#jeodegetmodpath)) use that mod's identity. 
Errors if no mod with `id` exists, the original identity is restored when `fn` finishes.

```lua
jeode.runAsMod("other-mod", function()
    print("Assumed identity of: " .. jeode.getModId()) -- "Assumed identity of: other-mod"
end)
```
