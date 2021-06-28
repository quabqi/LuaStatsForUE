// LuaStats.cpp
#include "LuaStats.h"
#include "UnLuaEx.h"
#include "Stats/Stats2.h"

DECLARE_STATS_GROUP(TEXT("Lua"), STATGROUP_Lua, STATCAT_Advanced);

class FLuaCycleCounter : FCycleCounter
{
    TStatId StatId;
    bool bStart;
public:
    FORCEINLINE_STATS FLuaCycleCounter(TStatId InStatId)
        : StatId(InStatId.GetRawPointer())
        , bStart(false)
    {
    }
    
    ~FLuaCycleCounter()
    {
        Stop();
    }

    void Start()
    {
        if (!bStart)
        {
            bStart = true;
            FCycleCounter::Start(StatId);
        }
    }

    void Stop()
    {
        if (bStart)
        {
            bStart = false;
            FCycleCounter::Stop();
        }
    }

    void Set(const uint32 Cycles) const
    {
        if (FThreadStats::IsCollectingData())
        {
            FThreadStats::AddMessage(StatId.GetName(), EStatOperation::Set, static_cast<int64>(Cycles), true);
        }
    }

    TStatId GetStatId() const
    {
        return StatId;
    }
};

class FLuaSimpleSecondsStat
{
public:
    FLuaSimpleSecondsStat(TStatId InStatId, double InScale = 1.0)
        : bStart(false)
        , StartTime(0)
        , StatId(InStatId)
        , Scale(InScale)
    {

    }

    ~FLuaSimpleSecondsStat()
    {
        Stop();
    }

    void Start()
    {
        bStart = true;
        StartTime = FPlatformTime::Seconds();
    }

    void Stop()
    {
        if (bStart)
        {
            bStart = false;
            const double TotalTime = (FPlatformTime::Seconds() - StartTime) * Scale;
            FThreadStats::AddMessage(StatId.GetName(), EStatOperation::Add, TotalTime);
        }
    }

private:
    bool bStart;
    double StartTime;
    TStatId StatId;
    double Scale;
};

class FLuaStats
{
private:
    TSparseArray<FLuaCycleCounter> CycleCounters;
    TMap<FName, int32> NameToCycleCounter;
    TMap<TStatIdData const*, int32> PtrToCycleCounter;
    TArray<int32> CycleCounterStack;

    TSparseArray<FLuaSimpleSecondsStat> SimpleSecondsStats;
    TMap<FName, int32> NameToSecondsStat;
    TMap<TStatIdData const*, int32> PtrToSecondsStat;

    TMap<FName, TStatIdData const*> Int64Stats;
    TMap<FName, TStatIdData const*> DoubleStats;
    TMap<FName, TStatIdData const*> MemoryStats;

    static TStatId CreateStatId(FName StatName, const TCHAR* StatDesc, bool bShouldClearEveryFrame,
        EStatDataType::Type InStatType, bool bCycleStat,
        FPlatformMemory::EMemoryCounterRegion MemRegion = FPlatformMemory::MCR_Invalid);

    void StartCycleCounterInternal(int32 Index);
    void SetCycleCounterInternal(int32 Index, const uint32 Cycles);
    void StartSimpleSecondsInternal(int32 Index);
    void StopSimpleSecondsInternal(int32 Index);
public:

    TStatIdData const* CreateCycleCounter(FName StatName, const TCHAR* StatDesc = nullptr);
    TStatIdData const* CreateSimpleSeconds(FName StatName, const TCHAR* StatDesc = nullptr, double InScale = 1.0);
    TStatIdData const* CreateInt64Counter(FName StatName, const TCHAR* StatDesc = nullptr);
    TStatIdData const* CreateInt64Accumulator(FName StatName, const TCHAR* StatDesc = nullptr);
    TStatIdData const* CreateDoubleCounter(FName StatName, const TCHAR* StatDesc = nullptr);
    TStatIdData const* CreateDoubleAccumulator(FName StatName, const TCHAR* StatDesc = nullptr);
    TStatIdData const* CreateMemoryStat(FName StatName, const TCHAR* StatDesc = nullptr);

    bool StartCycleCounter(FName StatName);
    bool StartCycleCounter(TStatIdData const* StatIdPtr);
    bool StopCycleCounter();
    bool SetCycleCounter(FName StatName, const uint32 Cycles);
    bool SetCycleCounter(TStatIdData const* StatIdPtr, const uint32 Cycles);
    
    bool StartSimpleSeconds(FName StatName);
    bool StartSimpleSeconds(TStatIdData const* StatIdPtr);
    bool StopSimpleSeconds(FName StatName);
    bool StopSimpleSeconds(TStatIdData const* StatIdPtr);
    
    bool AddInt64Stat(FName StatName, int64 Value) const;
    bool AddInt64Stat(TStatIdData const* StatIdPtr, int64 Value) const;
    bool SubtractInt64Stat(FName StatName, int64 Value) const;
    bool SubtractInt64Stat(TStatIdData const* StatIdPtr, int64 Value) const;
    bool SetInt64Stat(FName StatName, int64 Value) const;
    bool SetInt64Stat(TStatIdData const* StatIdPtr, int64 Value) const;
    
    bool AddMemoryStat(FName StatName, int64 Value) const;
    bool AddMemoryStat(TStatIdData const* StatIdPtr, int64 Value) const;
    bool SubtractMemoryStat(FName StatNam, int64 Value) const;
    bool SubtractMemoryStat(TStatIdData const* StatIdPtr, int64 Value) const;
    bool SetMemoryStat(FName StatName, int64 Value) const;
    bool SetMemoryStat(TStatIdData const* StatIdPtr, int64 Value) const;
    
    bool AddDoubleStat(FName StatName, double Value) const;
    bool AddDoubleStat(TStatIdData const* StatIdPtr, double Value) const;
    bool SubtractDoubleStat(FName StatName, double Value) const;
    bool SubtractDoubleStat(TStatIdData const* StatIdPtr, double Value) const;
    bool SetDoubleStat(FName StatName, double Value) const;
    bool SetDoubleStat(TStatIdData const* StatIdPtr, double Value) const;
    
    bool SetFNameStat(FName StatName, const char* Value) const;
    bool SetFNameStat(TStatIdData const* StatIdPtr, const char* Value) const;
};

bool FLuaStats::AddInt64Stat(TStatIdData const* StatIdPtr, int64 Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && Int64Stats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Add, Value);
        TRACE_STAT_ADD(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::AddInt64Stat(FName StatName, int64 Value) const
{
    if (Value != 0 && Int64Stats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Add, Value);
        TRACE_STAT_ADD(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::SubtractInt64Stat(FName StatName, int64 Value) const
{
    if (Value != 0 && Int64Stats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Subtract, Value);
        TRACE_STAT_ADD(StatName, -Value);
        return true;
    }
    return false;
}

bool FLuaStats::SubtractInt64Stat(TStatIdData const* StatIdPtr, int64 Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && Int64Stats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Subtract, Value);
        TRACE_STAT_ADD(StatName, -Value);
        return true;
    }
    return false;
}

bool FLuaStats::SetInt64Stat(FName StatName, int64 Value) const
{
    if (Value != 0 && Int64Stats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Set, Value);
        TRACE_STAT_SET(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::SetInt64Stat(TStatIdData const* StatIdPtr, int64 Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && Int64Stats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Set, Value);
        TRACE_STAT_SET(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::AddMemoryStat(FName StatName, int64 Value) const
{
    if (Value != 0 && MemoryStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Add, Value);
        TRACE_STAT_ADD(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::AddMemoryStat(TStatIdData const* StatIdPtr, int64 Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && MemoryStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Add, Value);
        TRACE_STAT_ADD(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::SubtractMemoryStat(FName StatName, int64 Value) const
{
    if (Value != 0 && MemoryStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Subtract, Value);
        TRACE_STAT_ADD(StatName, -Value);
        return true;
    }
    return false;
}

bool FLuaStats::SubtractMemoryStat(TStatIdData const* StatIdPtr, int64 Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && MemoryStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Subtract, Value);
        TRACE_STAT_ADD(StatName, -Value);
        return true;
    }
    return false;
}

bool FLuaStats::SetMemoryStat(FName StatName, int64 Value) const
{
    if (Value != 0 && MemoryStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Set, Value);
        TRACE_STAT_SET(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::SetMemoryStat(TStatIdData const* StatIdPtr, int64 Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && MemoryStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Set, Value);
        TRACE_STAT_SET(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::AddDoubleStat(FName StatName, double Value) const
{
    if (Value != 0 && DoubleStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Add, Value);
        TRACE_STAT_ADD(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::AddDoubleStat(TStatIdData const* StatIdPtr, double Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && DoubleStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Add, Value);
        TRACE_STAT_ADD(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::SubtractDoubleStat(FName StatName, double Value) const
{
    if (Value != 0 && DoubleStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Subtract, Value);
        TRACE_STAT_ADD(StatName, -Value);
        return true;
    }
    return false;
}

bool FLuaStats::SubtractDoubleStat(TStatIdData const* StatIdPtr, double Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && DoubleStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Subtract, Value);
        TRACE_STAT_ADD(StatName, -Value);
        return true;
    }
    return false;
}

bool FLuaStats::SetDoubleStat(FName StatName, double Value) const
{
    if (Value != 0 && DoubleStats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Set, Value);
        TRACE_STAT_SET(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::SetDoubleStat(TStatIdData const* StatIdPtr, double Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != 0 && Int64Stats.Contains(StatName) && FThreadStats::IsCollectingData())
    {
        FThreadStats::AddMessage(StatName, EStatOperation::Set, Value);
        TRACE_STAT_SET(StatName, Value);
        return true;
    }
    return false;
}

bool FLuaStats::SetFNameStat(FName StatName, const char* Value) const
{
    if (Value != nullptr)
    {
        FThreadStats::AddMessage(StatName, EStatOperation::SpecialMessageMarker, FName(Value));
        return true;
    }
    return false;
}

bool FLuaStats::SetFNameStat(TStatIdData const* StatIdPtr, const char* Value) const
{
    const FName StatName = MinimalNameToName(StatIdPtr->Name);
    if (Value != nullptr)
    {
        FThreadStats::AddMessage(StatName, EStatOperation::SpecialMessageMarker, FName(Value));
        return true;
    }
    return false;
}

TStatId FLuaStats::CreateStatId(FName StatName, const TCHAR* StatDesc, bool bShouldClearEveryFrame,
    EStatDataType::Type InStatType, bool bCycleStat, FPlatformMemory::EMemoryCounterRegion MemRegion)
{
    FStartupMessages::Get().AddMetadata(StatName, StatDesc,
        FStatGroup_STATGROUP_Lua::GetGroupName(),
        FStatGroup_STATGROUP_Lua::GetGroupCategory(),
        FStatGroup_STATGROUP_Lua::GetDescription(), bShouldClearEveryFrame,
        InStatType, bCycleStat, FStatGroup_STATGROUP_Lua::GetSortByName(), MemRegion);

    const TStatId StatID = IStatGroupEnableManager::Get().GetHighPerformanceEnableForStat(StatName,
        FStatGroup_STATGROUP_Lua::GetGroupName(),
        FStatGroup_STATGROUP_Lua::GetGroupCategory(),
        FStatGroup_STATGROUP_Lua::IsDefaultEnabled(),
        bShouldClearEveryFrame, InStatType,
        nullptr, bCycleStat,
        FStatGroup_STATGROUP_Lua::GetSortByName(), MemRegion);
    return StatID;
}

TStatIdData const* FLuaStats::CreateCycleCounter(FName StatName, const TCHAR* StatDesc)
{
    if (NameToCycleCounter.Contains(StatName))
    {
        return nullptr;
    }
    TStatId Result = CreateStatId(StatName, StatDesc, true, EStatDataType::ST_int64, true);
    int32 Index = CycleCounters.Emplace(Result);
    NameToCycleCounter.Emplace(StatName, Index);
    PtrToCycleCounter.Emplace(Result.GetRawPointer(), Index);
    return Result.GetRawPointer();
}

void FLuaStats::StartCycleCounterInternal(int32 Index)
{
    check(CycleCounters.IsValidIndex(Index));
    CycleCounterStack.Push(Index);
    CycleCounters[Index].Start();
}

void FLuaStats::SetCycleCounterInternal(int32 Index, const uint32 Cycles)
{
    check(CycleCounters.IsValidIndex(Index));
    CycleCounters[Index].Set(Cycles);
}

bool FLuaStats::StartCycleCounter(FName StatName)
{
    if (const auto Result = NameToCycleCounter.Find(StatName))
    {
        StartCycleCounterInternal(*Result);
        return true;
    }
    return false;
}

bool FLuaStats::StartCycleCounter(TStatIdData const* StatIdPtr)
{
    if (const auto Result = PtrToCycleCounter.Find(StatIdPtr))
    {
        StartCycleCounterInternal(*Result);
        return true;
    }
    return false;
}

bool FLuaStats::StopCycleCounter()
{
    if (CycleCounterStack.Num() == 0)
    {
        return false;
    }
    const auto Index = CycleCounterStack.Pop();
    check(CycleCounters.IsValidIndex(Index));
    CycleCounters[Index].Stop();
    return true;
}

bool FLuaStats::SetCycleCounter(FName StatName, const uint32 Cycles)
{
    if (const auto Result = NameToCycleCounter.Find(StatName))
    {
        SetCycleCounterInternal(*Result, Cycles);
        return true;
    }
    return false;
}

bool FLuaStats::SetCycleCounter(TStatIdData const* StatIdPtr, const uint32 Cycles)
{
    if (const auto Result = PtrToCycleCounter.Find(StatIdPtr))
    {
        SetCycleCounterInternal(*Result, Cycles);
        return true;
    }
    return false;
}

TStatIdData const* FLuaStats::CreateSimpleSeconds(FName StatName, const TCHAR* StatDesc, double InScale)
{
    if (NameToSecondsStat.Contains(StatName))
    {
        return nullptr;
    }
    const TStatId StatId = CreateDoubleAccumulator(StatName, StatDesc);
    auto Index = SimpleSecondsStats.Emplace(StatId, InScale);
    NameToSecondsStat.Emplace(StatName, Index);
    PtrToSecondsStat.Emplace(StatId.GetRawPointer(), Index);
    return StatId.GetRawPointer();
}

void FLuaStats::StartSimpleSecondsInternal(int32 Index)
{
    check(SimpleSecondsStats.IsValidIndex(Index));
    SimpleSecondsStats[Index].Start();
}

void FLuaStats::StopSimpleSecondsInternal(int32 Index)
{
    check(SimpleSecondsStats.IsValidIndex(Index));
    SimpleSecondsStats[Index].Stop();
}

bool FLuaStats::StartSimpleSeconds(FName StatName)
{
    if (const auto Result = NameToSecondsStat.Find(StatName))
    {
        StartSimpleSecondsInternal(*Result);
        return true;
    }
    return false;
}

bool FLuaStats::StartSimpleSeconds(TStatIdData const* StatIdPtr)
{
    if (const auto Result = PtrToSecondsStat.Find(StatIdPtr))
    {
        StartSimpleSecondsInternal(*Result);
        return true;
    }
    return false;
}

bool FLuaStats::StopSimpleSeconds(FName StatName)
{
    if (const auto Result = NameToSecondsStat.Find(StatName))
    {
        StopSimpleSecondsInternal(*Result);
        return true;
    }
    return false;
}

bool FLuaStats::StopSimpleSeconds(TStatIdData const* StatIdPtr)
{
    if (const auto Result = PtrToSecondsStat.Find(StatIdPtr))
    {
        StopSimpleSecondsInternal(*Result);
        return true;
    }
    return false;
}

TStatIdData const* FLuaStats::CreateInt64Counter(FName StatName, const TCHAR* StatDesc)
{
    if (Int64Stats.Contains(StatName))
    {
        return nullptr;
    }
    const TStatId StatId = CreateStatId(StatName, StatDesc, true, EStatDataType::ST_int64, false);
    Int64Stats.Emplace(StatName, StatId.GetRawPointer());
    return StatId.GetRawPointer();
}

TStatIdData const* FLuaStats::CreateInt64Accumulator(FName StatName, const TCHAR* StatDesc)
{
    if (Int64Stats.Contains(StatName))
    {
        return nullptr;
    }
    const TStatId StatId = CreateStatId(StatName, StatDesc, false, EStatDataType::ST_int64, false);
    Int64Stats.Emplace(StatName, StatId.GetRawPointer());
    return StatId.GetRawPointer();
}

TStatIdData const* FLuaStats::CreateDoubleCounter(FName StatName, const TCHAR* StatDesc)
{
    if (DoubleStats.Contains(StatName))
    {
        return nullptr;
    }
    const TStatId StatId = CreateStatId(StatName, StatDesc, true, EStatDataType::ST_double, false);
    DoubleStats.Emplace(StatName, StatId.GetRawPointer());
    return StatId.GetRawPointer();
}

TStatIdData const* FLuaStats::CreateDoubleAccumulator(FName StatName, const TCHAR* StatDesc)
{
    if (DoubleStats.Contains(StatName))
    {
        return nullptr;
    }
    const TStatId StatId = CreateStatId(StatName, StatDesc, false, EStatDataType::ST_double, false);
    DoubleStats.Emplace(StatName, StatId.GetRawPointer());
    return StatId.GetRawPointer();
}

TStatIdData const* FLuaStats::CreateMemoryStat(FName StatName, const TCHAR* StatDesc)
{
    if (MemoryStats.Contains(StatName))
    {
        return nullptr;
    }
    const TStatId StatId = CreateStatId(StatName, StatDesc, false, EStatDataType::ST_int64, false,
        FPlatformMemory::MCR_Physical);
    MemoryStats.Emplace(StatName, StatId.GetRawPointer());
    return StatId.GetRawPointer();
}

FLuaStats GLuaStats;

int32 CycleCounter_Create(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum <= 0 || ParamNum > 2)
    {
        lua_pushnil(L);
        return 1;
    }
    if (!lua_isstring(L, 1))
    {
        lua_pushnil(L);
        return 1;
    }

    FString StatDesc;
    if (ParamNum > 1 && lua_isstring(L, 2))
    {
        StatDesc = lua_tostring(L, 2);
    }
    const FName StatName = lua_tostring(L, 1);
    const auto StatIdPtr = GLuaStats.CreateCycleCounter(StatName, StatDesc.IsEmpty() ? nullptr : *StatDesc);
    if (StatIdPtr)
    {
        lua_pushlightuserdata(L, (void*)StatIdPtr);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 CycleCounter_Start(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
    }
    else if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.StartCycleCounter(lua_tostring(L, 1));
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.StartCycleCounter(static_cast<TStatIdData const*>(lua_touserdata(L, 1)));
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 CycleCounter_Stop(lua_State* L)
{
    const bool Result = GLuaStats.StopCycleCounter();
    lua_pushboolean(L, Result ? 1 : 0);
    return 1;
}

int32 CycleCounter_Set(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
    }
    else if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.StartCycleCounter(lua_tostring(L, 1));
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.StartCycleCounter(static_cast<TStatIdData const*>(lua_touserdata(L, 1)));
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 SimpleSeconds_Create(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum <= 0 || ParamNum > 2)
    {
        lua_pushnil(L);
        return 1;
    }
    if (!lua_isstring(L, 1))
    {
        lua_pushnil(L);
        return 1;
    }

    FString StatDesc;
    if (ParamNum > 1 && lua_isstring(L, 2))
    {
        StatDesc = lua_tostring(L, 2);
    }
    double Scale = 1.0;
    if (ParamNum > 2 && lua_isnumber(L, 3))
    {
        Scale = lua_tonumber(L, 3);
    }
    const FName StatName = lua_tostring(L, 1);
    const auto StatIdPtr = GLuaStats.CreateSimpleSeconds(StatName, StatDesc.IsEmpty() ? nullptr : *StatDesc, Scale);
    if (StatIdPtr)
    {
        lua_pushlightuserdata(L, (void*)StatIdPtr);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 SimpleSeconds_Start(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
    }
    else if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.StartSimpleSeconds(lua_tostring(L, 1));
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.StartSimpleSeconds(static_cast<TStatIdData const*>(lua_touserdata(L, 1)));
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 SimpleSeconds_Stop(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
    }
    else if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.StopSimpleSeconds(lua_tostring(L, 1));
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.StopSimpleSeconds(static_cast<TStatIdData const*>(lua_touserdata(L, 1)));
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 Int64Stat_Create(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum <= 0 || ParamNum > 2)
    {
        lua_pushnil(L);
        return 1;
    }
    if (!lua_isstring(L, 1))
    {
        lua_pushnil(L);
        return 1;
    }

    FString StatDesc;
    const FName StatName = lua_tostring(L, 1);
    if (ParamNum > 1 && lua_isstring(L, 2))
    {
        StatDesc = lua_tostring(L, 2);
    }
    bool bCounter = true;
    if (ParamNum > 2 && lua_isboolean(L, 3))
    {
        bCounter = (lua_toboolean(L, 3) != 0);
    }
    TStatIdData const* StatIdPtr;
    if (bCounter)
    {
        StatIdPtr = GLuaStats.CreateInt64Counter(StatName, StatDesc.IsEmpty() ? nullptr : *StatDesc);
    }
    else
    {
        StatIdPtr = GLuaStats.CreateInt64Accumulator(StatName, StatDesc.IsEmpty() ? nullptr : *StatDesc);
    }
    if (StatIdPtr)
    {
        lua_pushlightuserdata(L, (void*)StatIdPtr);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 Int64Stat_Add(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    int64 Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.AddInt64Stat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.AddInt64Stat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 Int64Stat_Subtract(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    int64 Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.SubtractInt64Stat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.SubtractInt64Stat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 Int64Stat_Set(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    int64 Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.SetInt64Stat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.SetInt64Stat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 DoubleStat_Create(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum <= 0 || ParamNum > 2)
    {
        lua_pushnil(L);
        return 1;
    }
    if (!lua_isstring(L, 1))
    {
        lua_pushnil(L);
        return 1;
    }

    FString StatDesc;
    const FName StatName = lua_tostring(L, 1);
    if (ParamNum > 1 && lua_isstring(L, 2))
    {
        StatDesc = lua_tostring(L, 2);
    }
    bool bCounter = true;
    if (ParamNum > 2 && lua_isboolean(L, 3))
    {
        bCounter = (lua_toboolean(L, 3) != 0);
    }
    TStatIdData const* StatIdPtr;
    if (bCounter)
    {
        StatIdPtr = GLuaStats.CreateDoubleCounter(StatName, StatDesc.IsEmpty() ? nullptr : *StatDesc);
    }
    else
    {
        StatIdPtr = GLuaStats.CreateDoubleAccumulator(StatName, StatDesc.IsEmpty() ? nullptr : *StatDesc);
    }
    if (StatIdPtr)
    {
        lua_pushlightuserdata(L, (void*)StatIdPtr);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;

}

int32 DoubleStat_Add(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    double Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.AddDoubleStat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.AddDoubleStat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 DoubleStat_Subtract(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    double Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.SubtractDoubleStat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.SubtractDoubleStat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 DoubleStat_Set(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    double Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.SetDoubleStat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.SetDoubleStat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 FNameStat_Set(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    const char* Value = nullptr;
    if (ParamNum >= 2 && lua_isstring(L, 2))
    {
        Value = lua_tostring(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.SetFNameStat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.SetFNameStat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 MemoryStat_Create(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum <= 0 || ParamNum > 2)
    {
        lua_pushnil(L);
        return 1;
    }
    if (!lua_isstring(L, 1))
    {
        lua_pushnil(L);
        return 1;
    }

    FString StatDesc;
    const FName StatName = lua_tostring(L, 1);
    if (ParamNum > 1 && lua_isstring(L, 2))
    {
        StatDesc = lua_tostring(L, 2);
    }

    TStatIdData const* StatIdPtr = GLuaStats.CreateMemoryStat(StatName, StatDesc.IsEmpty() ? nullptr : *StatDesc);
    if (StatIdPtr)
    {
        lua_pushlightuserdata(L, (void*)StatIdPtr);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;

}

int32 MemoryStat_Add(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    int64 Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.AddMemoryStat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.AddMemoryStat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 MemoryStat_Subtract(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    int64 Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.SubtractMemoryStat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.SubtractMemoryStat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

int32 MemoryStat_Set(lua_State* L)
{
    const int32 ParamNum = lua_gettop(L);
    if (ParamNum < 1)
    {
        lua_pushnil(L);
        return 1;
    }
    int64 Value = 1;
    if (ParamNum >= 2 && lua_isnumber(L, 2))
    {
        Value = lua_tonumber(L, 2);
    }
    if (lua_isstring(L, 1))
    {
        const bool Result = GLuaStats.SetMemoryStat(lua_tostring(L, 1), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else if (lua_islightuserdata(L, 1))
    {
        const bool Result = GLuaStats.SetMemoryStat(static_cast<TStatIdData const*>(lua_touserdata(L, 1)), Value);
        lua_pushboolean(L, Result ? 1 : 0);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

static const luaL_Reg CycleCounterLib[] =
{
    { "Create", CycleCounter_Create },
    { "Start", CycleCounter_Start },
    { "Stop", CycleCounter_Stop },
    { "Set", CycleCounter_Set },
    { nullptr, nullptr }
};

static const luaL_Reg SimpleSecondsLib[] =
{
    { "Create", SimpleSeconds_Create },
    { "Start", SimpleSeconds_Start },
    { "Stop", SimpleSeconds_Stop },
    { nullptr, nullptr }
};

static const luaL_Reg Int64StatLib[] =
{
    { "Create", Int64Stat_Create },
    { "Add", Int64Stat_Add },
    { "Subtract", Int64Stat_Subtract },
    { "Set", Int64Stat_Set },
    { nullptr, nullptr }
};

static const luaL_Reg DoubleStatLib[] =
{
    { "Create", DoubleStat_Create },
    { "Add", DoubleStat_Add },
    { "Subtract", DoubleStat_Subtract },
    { "Set", DoubleStat_Set },
    { nullptr, nullptr }
};

static const luaL_Reg FNameStatLib[] =
{
    { "Set", FNameStat_Set },
    { nullptr, nullptr }
};

static const luaL_Reg MemoryStatLib[] =
{
    { "Create", MemoryStat_Create },
    { "Add", MemoryStat_Add },
    { "Subtract", MemoryStat_Subtract },
    { "Set", MemoryStat_Set },
    { nullptr, nullptr }
};

EXPORT_UNTYPED_CLASS(FCycleCounter, false, CycleCounterLib)
IMPLEMENT_EXPORTED_CLASS(FCycleCounter)

EXPORT_UNTYPED_CLASS(FSimpleSeconds, false, SimpleSecondsLib)
IMPLEMENT_EXPORTED_CLASS(FSimpleSeconds)

EXPORT_UNTYPED_CLASS(FInt64Stat, false, Int64StatLib)
IMPLEMENT_EXPORTED_CLASS(FInt64Stat)

EXPORT_UNTYPED_CLASS(FDoubleStat, false, DoubleStatLib)
IMPLEMENT_EXPORTED_CLASS(FDoubleStat)

EXPORT_UNTYPED_CLASS(FNameStat, false, FNameStatLib)
IMPLEMENT_EXPORTED_CLASS(FNameStat)

EXPORT_UNTYPED_CLASS(FMemoryStat, false, MemoryStatLib)
IMPLEMENT_EXPORTED_CLASS(FMemoryStat)
