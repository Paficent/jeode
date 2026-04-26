--[[
Jeode autoupdating mod example, in theory all you'd need to distribute your mod is this script.
Do note that this form of distribution is a potential security issue, and is based around the
trust of the author and the security of their accounts.

- Paficent
]]

-- urlBase can be anything, but a GitHub repository is best practice
local urlBase = "https://raw.githubusercontent.com/Paficent/jeode/main/examples/auto-updater/"
local root, modules, sources = mod.getRoot(), {}, {}

local function fetch(f)
    local response = net.request(urlBase .. f)
    if not response or not response.ok then return end

    return response.body
end

-- Sources could be defined by Lua (see commented), but if you wanted to add new files later, this is the better solution
sources = net.jsonDecode(fetch("sources.json"))
-- local sources = {"test.lua", "lib/another.lua", "data/notarealasset.txt"}

for _, src in ipairs(sources) do
    local content = fetch(src)

    -- caches the updated file
    if content and #content > 0 then
        file.write(root .. "/" .. src, content)
    end

    -- loads lua scripts via require
    if src:sub(-4) == ".lua" then
    	local script = src:gsub("%.lua$", "")
    	local ok, result = pcall(require, script)

    	if ok then
            modules[script] = result
    	else
            print(string.format("failed to load %s: %s", script, result))
    	end
    end
end

-- print(modules["lib/test"].success)
-- modules["lib/another"]()
