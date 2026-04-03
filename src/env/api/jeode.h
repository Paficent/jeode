#pragma once

struct LuaApiTable;
class ModLoader;

void jeode_api_init(const ModLoader *loader);
const LuaApiTable &jeode_api_table();
