#include "ModLoader.h"

#include "ModLoader.h"
#include "..\\Minecraft.World\\LevelHooks.h"
#include "..\\Minecraft.World\\ChunkBakeHooks.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

ModLoader g_modLoader;

// ---------------------------------------------------------------------------
// Trampoline: bridges the C function-pointer in ModHostAPI to g_modLoader.
// Must be a plain static function so its address is stable.
// ---------------------------------------------------------------------------

static void RegisterClientTickThunk(ClientTickFn fn)
{
    g_modLoader.registerClientTick(fn);
}

static void RegisterDynamicLightQueryThunk(QueryDynamicLightFn fn)
{
    g_modLoader.registerDynamicLightQuery(fn);
}

static void RegisterPrepareChunkLightSnapshotThunk(PrepareChunkLightSnapshotFn fn)
{
    g_modLoader.registerPrepareChunkLightSnapshot(fn);
}

static void RegisterQueryChunkSnapshotLightThunk(QueryChunkSnapshotLightFn fn)
{
    g_modLoader.registerQueryChunkSnapshotLight(fn);
}

static void RegisterDestroyChunkLightSnapshotThunk(DestroyChunkLightSnapshotFn fn)
{
    g_modLoader.registerDestroyChunkLightSnapshot(fn);
}

static void RegisterBeginEmitterFeedThunk(BeginEmitterFeedFn fn)
{
    g_modLoader.registerBeginEmitterFeed(fn);
}

static void RegisterNotifyEmitterThunk(NotifyEmitterFn fn)
{
    g_modLoader.registerNotifyEmitter(fn);
}

static void RegisterEndEmitterFeedThunk(EndEmitterFeedFn fn)
{
    g_modLoader.registerEndEmitterFeed(fn);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void modLog(const char* tag, const char* message)
{
    char buf[1024];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "[%s] %s\n", tag, message);
    OutputDebugStringA(buf);
}

void ModLoader::hostLog(const char* message)
{
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

// ---------------------------------------------------------------------------
// loadAllMods
// ---------------------------------------------------------------------------

void ModLoader::loadAllMods(const char* modsDir)
{
    // Build the search pattern "mods\*.dll"
    char pattern[MAX_PATH];
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*.dll", modsDir);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern, &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        modLog("ModLoader", "No mods folder found or no .dll files present");
        return;
    }

    int found = 0;
    do
    {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        ++found;

        char fullPath[MAX_PATH];
        _snprintf_s(fullPath, sizeof(fullPath), _TRUNCATE, "%s\\%s", modsDir, findData.cFileName);

        // Log that we found a DLL
        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Loading %s", findData.cFileName);
            modLog("ModLoader", msg);
        }

        HMODULE hMod = LoadLibraryA(fullPath);
        if (!hMod)
        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Failed to load %s (error %lu)", findData.cFileName, GetLastError());
            modLog("ModLoader", msg);
            continue;
        }

        // Resolve entry point
        InitModFn initFn = reinterpret_cast<InitModFn>(
            GetProcAddress(hMod, "InitMod"));

        if (!initFn)
        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "%s has no InitMod export – skipping", findData.cFileName);
            modLog("ModLoader", msg);
            FreeLibrary(hMod);
            continue;
        }

        ShutdownModFn shutdownFn = reinterpret_cast<ShutdownModFn>(
            GetProcAddress(hMod, "ShutdownMod"));

        // Build the host API struct. Value-initialize ({}) so every slot is
        // zero before we fill in the fields we know about — prevents garbage
        // function pointers reaching the mod if the struct ever grows.
        ModHostAPI api{};
        api.apiVersion                       = MOD_API_VERSION;
        api.log                              = &ModLoader::hostLog;
        api.registerClientTick               = &RegisterClientTickThunk;
        api.registerDynamicLightQuery        = &RegisterDynamicLightQueryThunk;
        api.registerPrepareChunkLightSnapshot = &RegisterPrepareChunkLightSnapshotThunk;
        api.registerQueryChunkSnapshotLight   = &RegisterQueryChunkSnapshotLightThunk;
        api.registerDestroyChunkLightSnapshot = &RegisterDestroyChunkLightSnapshotThunk;
        api.getTileOpacity                   = [](int x, int y, int z) -> int
        {
            return g_hostGetTileOpacity ? g_hostGetTileOpacity(x, y, z) : 0;
        };
        api.markRegionDirty                  = [](int x0, int y0, int z0, int x1, int y1, int z1)
        {
            if (g_markRegionDirty) g_markRegionDirty(x0, y0, z0, x1, y1, z1);
        };
        api.registerBeginEmitterFeed         = &RegisterBeginEmitterFeedThunk;
        api.registerNotifyEmitter            = &RegisterNotifyEmitterThunk;
        api.registerEndEmitterFeed           = &RegisterEndEmitterFeedThunk;

        bool ok = initFn(&api);
        if (!ok)
        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "%s InitMod returned false – skipping", findData.cFileName);
            modLog("ModLoader", msg);
            FreeLibrary(hMod);
            continue;
        }

        LoadedMod entry;
        entry.name       = findData.cFileName;
        entry.handle     = static_cast<void*>(hMod);
        entry.shutdownFn = shutdownFn;
        m_mods.push_back(entry);

        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Successfully loaded %s", findData.cFileName);
            modLog("ModLoader", msg);
        }

    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    // Summary
    {
        char msg[128];
        _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Found %d mod(s), loaded %zu", found, m_mods.size());
        modLog("ModLoader", msg);
    }

    // Publish the dynamic light provider (if any) into the engine hook slot
    // so Level.cpp can call through it without depending on Minecraft.Mods.
    if (m_dynamicLightQuery)
        g_queryDynamicLight = [](int x, int y, int z) -> int { return g_modLoader.queryDynamicLight(x, y, z); };
    else
        g_queryDynamicLight = nullptr;

    if (m_prepareChunkLightSnapshot)
        g_prepareChunkLightSnapshot = [](int cx, int cy, int cz) -> void* { return g_modLoader.prepareChunkLightSnapshot(cx, cy, cz); };
    else
        g_prepareChunkLightSnapshot = nullptr;

    if (m_queryChunkSnapshotLight)
        g_queryChunkSnapshotLight = [](void* s, int x, int y, int z) -> int { return g_modLoader.queryChunkSnapshotLight(s, x, y, z); };
    else
        g_queryChunkSnapshotLight = nullptr;

    if (m_destroyChunkLightSnapshot)
        g_destroyChunkLightSnapshot = [](void* s) { g_modLoader.destroyChunkLightSnapshot(s); };
    else
        g_destroyChunkLightSnapshot = nullptr;

    if (m_beginEmitterFeed)
        g_beginEmitterFeed = []() { g_modLoader.dispatchBeginEmitterFeed(); };
    else
        g_beginEmitterFeed = nullptr;

    if (m_notifyEmitter)
        g_notifyEmitter = [](int id, int x, int y, int z, int s) { g_modLoader.dispatchNotifyEmitter(id, x, y, z, s); };
    else
        g_notifyEmitter = nullptr;

    if (m_endEmitterFeed)
        g_endEmitterFeed = []() { g_modLoader.dispatchEndEmitterFeed(); };
    else
        g_endEmitterFeed = nullptr;
}

// ---------------------------------------------------------------------------
// unloadAllMods
// ---------------------------------------------------------------------------

void ModLoader::unloadAllMods()
{
    for (auto it = m_mods.rbegin(); it != m_mods.rend(); ++it)
    {
        if (it->shutdownFn)
            it->shutdownFn();

        FreeLibrary(static_cast<HMODULE>(it->handle));

        char msg[512];
        _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Unloaded %s", it->name.c_str());
        modLog("ModLoader", msg);
    }
    m_mods.clear();
    m_clientTickCallbacks.clear();
    m_dynamicLightQuery         = nullptr;
    m_prepareChunkLightSnapshot = nullptr;
    m_queryChunkSnapshotLight   = nullptr;
    m_destroyChunkLightSnapshot = nullptr;
    m_beginEmitterFeed          = nullptr;
    m_notifyEmitter             = nullptr;
    m_endEmitterFeed            = nullptr;
    g_queryDynamicLight         = nullptr;
    g_prepareChunkLightSnapshot = nullptr;
    g_queryChunkSnapshotLight   = nullptr;
    g_destroyChunkLightSnapshot = nullptr;
    g_beginEmitterFeed          = nullptr;
    g_notifyEmitter             = nullptr;
    g_endEmitterFeed            = nullptr;
}

// ---------------------------------------------------------------------------
// registerClientTick / tickClient
// ---------------------------------------------------------------------------

void ModLoader::registerClientTick(ClientTickFn fn)
{
    if (fn)
        m_clientTickCallbacks.push_back(fn);
}

void ModLoader::tickClient()
{
    for (ClientTickFn fn : m_clientTickCallbacks)
        fn();
}

// ---------------------------------------------------------------------------
// registerDynamicLightQuery / queryDynamicLight
// ---------------------------------------------------------------------------

void ModLoader::registerDynamicLightQuery(QueryDynamicLightFn fn)
{
    if (fn)
        m_dynamicLightQuery = fn;
}

int ModLoader::queryDynamicLight(int x, int y, int z) const
{
    if (!m_dynamicLightQuery)
        return 0;
    return m_dynamicLightQuery(x, y, z);
}

// ---------------------------------------------------------------------------
// Chunk bake snapshot API
// ---------------------------------------------------------------------------

void ModLoader::registerPrepareChunkLightSnapshot(PrepareChunkLightSnapshotFn fn)
{
    if (fn) m_prepareChunkLightSnapshot = fn;
}

void ModLoader::registerQueryChunkSnapshotLight(QueryChunkSnapshotLightFn fn)
{
    if (fn) m_queryChunkSnapshotLight = fn;
}

void ModLoader::registerDestroyChunkLightSnapshot(DestroyChunkLightSnapshotFn fn)
{
    if (fn) m_destroyChunkLightSnapshot = fn;
}

void* ModLoader::prepareChunkLightSnapshot(int chunkX, int chunkY, int chunkZ) const
{
    if (!m_prepareChunkLightSnapshot)
        return nullptr;
    return m_prepareChunkLightSnapshot(chunkX, chunkY, chunkZ);
}

int ModLoader::queryChunkSnapshotLight(void* snapshot, int x, int y, int z) const
{
    if (!m_queryChunkSnapshotLight || !snapshot)
        return 0;
    return m_queryChunkSnapshotLight(snapshot, x, y, z);
}

void ModLoader::destroyChunkLightSnapshot(void* snapshot) const
{
    if (m_destroyChunkLightSnapshot && snapshot)
        m_destroyChunkLightSnapshot(snapshot);
}

// ---------------------------------------------------------------------------
// Emitter feed
// ---------------------------------------------------------------------------

void ModLoader::registerBeginEmitterFeed(BeginEmitterFeedFn fn)
{
    if (fn) m_beginEmitterFeed = fn;
}

void ModLoader::registerNotifyEmitter(NotifyEmitterFn fn)
{
    if (fn) m_notifyEmitter = fn;
}

void ModLoader::registerEndEmitterFeed(EndEmitterFeedFn fn)
{
    if (fn) m_endEmitterFeed = fn;
}
