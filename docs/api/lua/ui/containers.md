---
icon: lucide/package
---

# Containers

Containers hold the rest of your interface. A [window](#uiwindow) is the floating panel everything lives in, and a [child](#uichild) is a smaller scrollable region you can nest inside it.

Both take a **body**, a function that draws their contents. You can pass just the body, or an options table followed by the body.

---

## ui.window

```luau
type WindowOpts = {
    noTitleBar: boolean?,
    noBackground: boolean?,
    noCollapse: boolean?,
    noMove: boolean?,
    noResize: boolean?,
    noScrollbar: boolean?,
    noClose: boolean?,
    opened: State<boolean>?,
}

ui.window(title: string, body: () -> ()) -> WindowResult
ui.window(title: string, opts: WindowOpts, body: () -> ()) -> WindowResult
```

Draws a window with the given title and runs `body` to fill it. The result has three events: `opened` (the window is open and not collapsed), `closed` (the close button was just pressed), and `hovered`.

```lua
ui.window("Settings", function()
    ui.text("Drawn inside the window")
end)
```

Pass an `opened` state to give the window a close button and track whether it's open. When the player closes it, the state flips to `false` for you.

```lua
local shown = ui.state(true)

ui.register(function()
    ui.window("Closable", { opened = shown }, function()
        ui.text("X button in top right corner will appear")
    end)
end)

shown:onChange(function(visible)
    print("The X button was clicked. The next frame the window will stop rendering.")
end)
```

The other options are flags that strip away parts of the window:

```lua
ui.window("Plain", { noTitleBar = true, noResize = true }, function()
    ui.text("No title bar, can't be resized")
end)
```

---

## ui.child

```luau
type ChildOpts = {
    border: boolean?,
}

ui.child(width: number, height: number, body: () -> ()) -> ChildResult
ui.child(width: number, height: number, opts: ChildOpts, body: () -> ()) -> ChildResult
```

Creates a region `width` by `height` pixels inside the current window. Pass `0` for a dimension to let it stretch to fit. The result has a single `hovered` event.

```lua
ui.window("Log", function()
    ui.child(0, 200, { border = true }, function()
        for i = 1, 50 do
            ui.text("line " .. i)
        end
    end)
end)
```
