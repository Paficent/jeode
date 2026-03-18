--
local MAX_DEPTH = 15

local SpecialCharacters = {
    ["\a"] = "\\a",
    ["\b"] = "\\b",
    ["\f"] = "\\f",
    ["\n"] = "\\n",
    ["\r"] = "\\r",
    ["\t"] = "\\t",
    ["\v"] = "\\v",
    ["\0"] = "\\0",
}

local Keywords = {
    ["and"] = true,
    ["break"] = true,
    ["do"] = true,
    ["else"] = true,
    ["elseif"] = true,
    ["end"] = true,
    ["false"] = true,
    ["for"] = true,
    ["function"] = true,
    ["if"] = true,
    ["in"] = true,
    ["local"] = true,
    ["nil"] = true,
    ["not"] = true,
    ["or"] = true,
    ["repeat"] = true,
    ["return"] = true,
    ["then"] = true,
    ["true"] = true,
    ["until"] = true,
    ["while"] = true,
    ["continue"] = true,
}

local function SerializeType(Value, Class)
    if Class == "string" then
        return string.format('"%s"', string.gsub(Value, "[%c%z]", SpecialCharacters))
    elseif Class == "function" then
        local functionType = (pcall(setfenv, Value, getfenv(Value)) and "Lua" or "C")
        local ok, info = pcall(debug.getinfo, Value)
        info = ok and info or {}
        if functionType == "Lua" then
            return string.format('"%s %s [%s Params & %s Upvalues]"',
                functionType, tostring(Value),
                tostring(info.nparams), tostring(info.nups))
        end
        return string.format('"%s %s"', functionType, tostring(Value))
    elseif Class == "userdata" then
        return "newproxy(" .. tostring(not (not getmetatable(Value))) .. ")"
    elseif Class == "thread" then
        return "'" .. tostring(Value) .. ", status: " .. coroutine.status(Value) .. "'"
    else
        return tostring(Value)
    end
end

local function CloneTable(tbl, seen)
    seen = seen or {}
    if seen[tbl] then
        return {}
    end
    seen[tbl] = true
    local out = {}
    for k, v in pairs(tbl) do
        out[k] = type(v) == "table" and CloneTable(v, seen) or v
    end
    return out
end

local function TableToString(Table, IgnoredTables, DepthData, Path)
    IgnoredTables = IgnoredTables or {}
    local CyclicData = IgnoredTables[Table]
    if CyclicData then
        return ((CyclicData[1] == DepthData[1] - 1 and "'[Cyclic Parent " or "'[Cyclic ") ..
            tostring(Table) .. ", path: " .. CyclicData[2] .. "]'")
    end

    Path = Path or "ROOT"
    DepthData = DepthData or { 0, Path }
    local Depth = DepthData[1] + 1

    if Depth > MAX_DEPTH then
        return "'[max depth]'"
    end

    DepthData[1] = Depth
    DepthData[2] = Path

    IgnoredTables[Table] = DepthData
    local Tab = string.rep("    ", Depth)
    local TrailingTab = string.rep("    ", Depth - 1)
    local Result = "{"

    local LineTab = "\n" .. Tab
    local HasOrder = true
    local Index = 1
    local IsEmpty = true

    for Key, Value in next, Table do
        IsEmpty = false
        if Index ~= Key then
            HasOrder = false
        else
            Index = Index + 1
        end

        local KeyClass, ValueClass = type(Key), type(Value)
        local HasBrackets = false

        if KeyClass == "string" then
            Key = string.gsub(Key, "[%c%z]", SpecialCharacters)
            if Keywords[Key] or not string.match(Key, "^[_%a][_%w]*$") then
                HasBrackets = true
                Key = string.format('["%s"]', Key)
            end
        else
            HasBrackets = true
            Key = "[" ..
                (KeyClass == "table"
                    and string.gsub(TableToString(Key, IgnoredTables, { Depth, Path }), "^%s*(.-)%s*$", "%1")
                    or SerializeType(Key, KeyClass)) ..
                "]"
        end

        if ValueClass == "table" and debug.getmetatable(Value) then
            local cloned = CloneTable(Value)
            cloned.__metatable = debug.getmetatable(Value)
            Value = cloned
        end

        Value = ValueClass == "table"
            and TableToString(Value, IgnoredTables, { Depth, Path }, Path .. (HasBrackets and "" or ".") .. Key)
            or SerializeType(Value, ValueClass)

        Result = Result .. LineTab .. (HasOrder and Value or Key .. " = " .. Value) .. ","
    end

    return IsEmpty and Result .. "}" or string.sub(Result, 1, -2) .. "\n" .. TrailingTab .. "}"
end


local function dump(name, tbl)
    local path = mod.getRoot() .. "/" .. name .. ".lua"
    local ok, result = pcall(TableToString, tbl)
    if ok then
        result = "return " .. result .. "\n"
        file.write(path, result)
        print("[ok] " .. name .. " (" .. #result .. " bytes)")
    else
        file.write(path, "-- dump error: " .. tostring(result) .. "\n")
        print("[err] " .. name .. ": " .. tostring(result))
    end
end

print("Dumping...")
do
    if game then
        dump("game", game)
    end

    if lua_sys then
        dump("lua_sys", lua_sys)
    end

    local reg_ok, registry = pcall(debug.getregistry)
    if reg_ok and registry then
        dump("registry", registry)
    end

    local realG = getmetatable(_G)
    if realG then realG = realG.__index end
    if realG then
        dump("globals", realG)
    end
end
