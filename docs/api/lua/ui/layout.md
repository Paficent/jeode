---
icon: lucide/proportions
---

# Layout

By default every widget sits on its own line from top to bottom. These are helpers that allow mod developers to modify this behavior. None of them return a result.

---

## ui.separator

```luau
ui.separator() -> ()
```

Draws a thin horizontal line across the current window

---

## ui.sameLine

```luau
ui.sameLine(offset: number?, spacing: number?) -> ()
```

Keeps the next widget on the same line as the previous one, `offset` sets it to a fixed x position from the window's left edge and `spacing` sets the gap in pixels.

---

## ui.spacing

```luau
ui.spacing() -> ()
```

Adds a small vertical gap between widgets.

---

## ui.indent

```luau
ui.indent(body: () -> ()) -> ()
```

Indents everything drawn inside `body` to the right.

```lua
ui.text("Settings")
ui.indent(function()
    ui.text("Volume")
    ui.text("Brightness")
end)
```
