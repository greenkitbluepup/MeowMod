#pragma once

#include <string>
#include <vector>

#include "ModAPI.h"

struct LoadedMod
{
    std::string   name;
    void*         handle;
    ShutdownModFn shutdownFn;
};

class ModLoader
{
public:
    void loadAllMods(const char* modsDir = "mods");
    void unloadAllMods();

    // Client tick ----------------------------------------------------------
    void registerClientTick(ClientTickFn fn);
    void tickClient();

    // Runtime dynamic light query ------------------------------------------
    void registerDynamicLightQuery(QueryDynamicLightFn fn);
    int  queryDynamicLight(int x, int y, int z) const;

    // Chunk bake snapshot API ----------------------------------------------
    void  registerPrepareChunkLightSnapshot(PrepareChunkLightSnapshotFn fn);
    void  registerQueryChunkSnapshotLight(QueryChunkSnapshotLightFn fn);
    void  registerDestroyChunkLightSnapshot(DestroyChunkLightSnapshotFn fn);

    void* prepareChunkLightSnapshot(int chunkX, int chunkY, int chunkZ) const;
    int   queryChunkSnapshotLight(void* snapshot, int x, int y, int z) const;
    void  destroyChunkLightSnapshot(void* snapshot) const;

    // Emitter feed ---------------------------------------------------------
    void registerBeginEmitterFeed(BeginEmitterFeedFn fn);
    void registerNotifyEmitter(NotifyEmitterFn fn);
    void registerEndEmitterFeed(EndEmitterFeedFn fn);

    // Called by the hook slot lambdas in loadAllMods.
    void dispatchBeginEmitterFeed() const  { if (m_beginEmitterFeed) m_beginEmitterFeed(); }
    void dispatchNotifyEmitter(int id, int x, int y, int z, int s) const { if (m_notifyEmitter) m_notifyEmitter(id, x, y, z, s); }
    void dispatchEndEmitterFeed() const    { if (m_endEmitterFeed) m_endEmitterFeed(); }

    ~ModLoader() { unloadAllMods(); }

private:
    std::vector<LoadedMod>    m_mods;
    std::vector<ClientTickFn> m_clientTickCallbacks;
    QueryDynamicLightFn            m_dynamicLightQuery          = nullptr;
    PrepareChunkLightSnapshotFn    m_prepareChunkLightSnapshot  = nullptr;
    QueryChunkSnapshotLightFn      m_queryChunkSnapshotLight    = nullptr;
    DestroyChunkLightSnapshotFn    m_destroyChunkLightSnapshot  = nullptr;
    BeginEmitterFeedFn             m_beginEmitterFeed           = nullptr;
    NotifyEmitterFn                m_notifyEmitter              = nullptr;
    EndEmitterFeedFn               m_endEmitterFeed             = nullptr;

    static void hostLog(const char* message);
};

extern ModLoader g_modLoader;
