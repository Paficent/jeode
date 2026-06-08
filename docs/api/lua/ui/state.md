---
icon: lucide/square-function
---

# State

Because the interface is redrawn every frame, a widget needs a place to store its value between frames. In Jeode, it's a **state**, or a variable created with `ui.state` and passed to widgets like [checkboxes](widgets.md#uicheckbox), [sliders](widgets.md#uislider), and [inputs](widgets.md#uiinput).

---

## ui.state

**`ui.state` must be called outside of the `ui.register` callback function.**

```luau
type State<T> = {
    value: T,
    onChange: (self: State<T>, fn: (value: T) -> ()) -> (),
}

ui.state(initial: T?) -> State<T>
```

Creates a state holding `initial` (or `nil`).

```lua
-- states registered first
local enabled = ui.state(false)
local volume = ui.state(100)
local input = ui.state("")

-- ui registered second
ui.register(function()
    -- etc
end)
```

---

## state:onChange

```luau
state:onChange(fn: (value: T) -> ()) -> ()
```

Registers a function that runs whenever the value changes that passes the new value as an argument. Setting a state to the value it already has won't fire it. 

```lua
local volume = ui.state(50)

volume:onChange(function(new)
    print("volume is now " .. new)
end)

volume.value = 75 -- prints "volume is now 75"
```

---

## Reading and writing

There are two ways to get and set a state's value, use whichever suits you best. 

Through the `value` field:

```lua
local count = ui.state(0)

count.value = count.value + 1
print(count.value) -- 1
```

Or by calling the state like a function. The value is returned if no arguments are passed or modified the first argument:

```lua
local count = ui.state(0)

count(count() + 1)
print(count()) -- 1
```


---

## Comparing states

States can be compared against themselves just like normal variables would be:

```lua
local a = ui.state(10)
local b = ui.state(10)

print(a == b) -- true
```

However, comparing a state against a lua primative like a `number` will not behave properly. Instead you must retrieve the state's `value` first:
```lua
local a = ui.state(10)
local b = 10

print(a == b) -- false
print(a() == b) -- true
print(a.value == b) -- true
```
