---
icon: lucide/monitor
hide:
  - toc
---

# ui

The `ui` table allows mods to draw their own interfaces on top of the game. It's built on [Dear ImGui](https://github.com/ocornut/imgui) and follows the same **immediate mode** model.

Here's the smallest possible example:

```lua title="init.lua"
ui.register(function()
    ui.window("Simple example", function()
        if ui.button("Click me").clicked() then
            print("clicked")
        end
    end)
end)
```

---

## Drawing every frame

Your interface lives inside a function passed to `ui.register`. Jeode calls that function every frame (that overlays are toggled) and redraws your UI. Anything on screen (windows, buttons, separator, etc.) has to be called from inside the `ui.register` function.

```luau
ui.register(draw: () -> ()) -> number
```

Registers a draw function and returns an id you can use to remove it later.

```luau
ui.disconnect(id: number) -> ()
```

Removes a registered draw function from the queue.

```lua
local id = ui.register(function()
    ui.window("temporary", function()
        ui.text("will stop being drawn later")
    end)
end)

task.wait(10)
ui.disconnect(id)
```

!!! note "The overlay has to be open"
    Your draw function only runs while Jeode's overlay is visible. Players toggle it with the toggle key (++f1++ by default, see [config.json](../../../config/config.md)).

---

## Reading what happened

Most widgets return a **result** table. Each event on it (like `clicked` or `hovered`) is a *function* you call to get a `true`/`false`, since the answer can change from frame to frame.

```lua
local btn = ui.button("Save")
if btn.clicked() then
    print("saving")
end

-- compact version
if ui.button("Save").clicked() then
    print("saving")
end
```

Every **result** also has an `addTooltip` method, which shows a tooltip when the widget is hovered and returns the result back so you can chain it:

```luau
result:addTooltip(text: string) -> result
```

```lua
ui.button("Delete"):addTooltip("This can't be undone!")
```

---

The next step is learning about [`ui.state`](state.md)
