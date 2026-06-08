# Lua API Reference
This page contains the most up to date documentation for Jeode's bundled Lua API.

While Jeode and My Singing Monsters bundles Lua 5.1.4, we've elected to use Luau type annotations for this documentation. This means that the actual usage of API functions will look more like the provided examples than the function signatures.

___

## The Sandbox
In an abundance of caution, several of Lua 5.1's features have been disabled or had their behavior modified for safety.

These blocked globals include: `loadfile`, `dofile`, `load`, `package.*` `os.execute`, `os.remove`, `os.rename`, `os.exit`, `os.tmpname`, and more.

___

## Globals
These are the functions and values your mod can use directly, that do not have a table in front of them.

Normal Lua 5.1 built-ins like `string`, `table`, `debug`, `math`, `coroutine`, `pairs`, `tostring`, etc. still exist. These are just the globals Jeode adds or modifies.

### `require`

```luau
require: <T>(name: string) -> T | true
```

Loads another Lua file from **your mod's folder**, runs it, caches it, and returns what the file returned (or `true` if it returned nothing).

Use dots **or** slashes for folders, `.lua` is already appended, errors if the file is not found.

```lua title="utils.lua"
local utils = {}
function utils.double(n) return n * 2 end
return utils
```

```lua title="init.lua"
local utils = require("utils")
print(utils.double(21)) -- 42
```

!!! warning "Sandboxed"
    `require` can only reach files inside your own mod folder.

### `loadstring`

```luau
loadstring(code: string, chunkName: string?) -> function | nil
```

Transforms a string of Lua source code into a function. `chunkName` is used for error messages and defaults to `("loadstring")`. Returns nil and errors on failure.

```lua
local fn, err = loadstring("return 2 + 2")
if fn then
    print(fn()) -- 4
else
    print("Error: " .. err)
end
```

!!! warning "Sandboxed"
    `loadstring` has been modified to only accept Lua source code and to reject bytecode.

### `shared`

```luau
shared: {any}
```

A table that is **shared between all mods**, useful for inter-mod communication.

```lua
-- in one mod
shared.playerNickname = "Riff"

-- in another mod
print(shared.playerNickname) -- "Riff"
```

### `_MOD`

```luau
_MOD = {
  id: string,
  root: string,
}
```

A table used internally for tracking information about your mod. 

The same information is also available from
[`jeode.getModId`](jeode.md#jeodegetmodid) and
[`jeode.getModPath`](jeode.md#jeodegetmodpath). It is recommended to use these functions instead.
