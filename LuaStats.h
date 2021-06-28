// LuaStats.h
#pragma once

#include "CoreMinimal.h"
#include "lua.hpp"

int32 CycleCounter_Create(lua_State* L);
int32 CycleCounter_Start(lua_State* L);
int32 CycleCounter_Stop(lua_State* L);
int32 CycleCounter_Set(lua_State* L);

int32 SimpleSeconds_Create(lua_State* L);
int32 SimpleSeconds_Start(lua_State* L);
int32 SimpleSeconds_Stop(lua_State* L);

int32 Int64Stat_Create(lua_State* L);
int32 Int64Stat_Add(lua_State* L);
int32 Int64Stat_Subtract(lua_State* L);
int32 Int64Stat_Set(lua_State* L);

int32 DoubleStat_Create(lua_State* L);
int32 DoubleStat_Add(lua_State* L);
int32 DoubleStat_Subtract(lua_State* L);
int32 DoubleStat_Set(lua_State* L);

int32 FNameStat_Set(lua_State* L);

int32 MemoryStat_Create(lua_State* L);
int32 MemoryStat_Add(lua_State* L);
int32 MemoryStat_Subtract(lua_State* L);
int32 MemoryStat_Set(lua_State* L);
