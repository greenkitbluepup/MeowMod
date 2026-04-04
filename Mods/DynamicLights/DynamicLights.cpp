#include "ModAPI.h"
#include "ModAPI.h"
#include "DynamicLightManager.h"

static ModHostAPI           g_api{};
static DynamicLightManager  g_lights;

// ---------------------------------------------------------------------------
// Emitter feed — game thread, inside Level::tickEntities
// ---------------------------------------------------------------------------

static void BeginEmitterFeed()
{
    g_lights.beginFeed();
}

static void NotifyEmitter(int entityId, int x, int y, int z, int strength)
{
    g_lights.notifyEmitter(entityId, x, y, z, strength);
}

static void EndEmitterFeed()
{
    g_lights.endFeed();
}

// ---------------------------------------------------------------------------
// Runtime dynamic light query — called by Level::getLightColor/getBrightness
// ---------------------------------------------------------------------------

static int QueryDynamicLight(int x, int y, int z)
{
    return g_lights.getDynamicContribution(x, y, z);
}

// ---------------------------------------------------------------------------
// Chunk bake snapshot API — called on game thread before rebuild dispatch
// ---------------------------------------------------------------------------

static void* PrepareChunkLightSnapshot(int chunkX, int chunkY, int chunkZ)
{
    return g_lights.takeSnapshot(chunkX, chunkY, chunkZ);
}

static int QueryChunkSnapshotLight(void* snapshot, int x, int y, int z)
{
    // Read-only; bake thread safe.
    const auto* snap = static_cast<const DynamicLightSnapshot*>(snapshot);
    return snap->getDynamicContribution(x, y, z);
}

static void DestroyChunkLightSnapshot(void* snapshot)
{
    delete static_cast<DynamicLightSnapshot*>(snapshot);
}

// ---------------------------------------------------------------------------
// Entry / exit
// ---------------------------------------------------------------------------

extern "C" __declspec(dllexport) bool InitMod(ModHostAPI* api)
{
    if (!api || api->apiVersion != MOD_API_VERSION)
        return false;

    g_api = *api;
    g_api.log("[DynamicLightMod] InitMod");

    g_lights.setTileOpacityFn(api->getTileOpacity);
    g_lights.setMarkRegionDirtyFn(api->markRegionDirty);

    g_api.registerDynamicLightQuery(&QueryDynamicLight);
    g_api.registerPrepareChunkLightSnapshot(&PrepareChunkLightSnapshot);
    g_api.registerQueryChunkSnapshotLight(&QueryChunkSnapshotLight);
    g_api.registerDestroyChunkLightSnapshot(&DestroyChunkLightSnapshot);
    g_api.registerBeginEmitterFeed(&BeginEmitterFeed);
    g_api.registerNotifyEmitter(&NotifyEmitter);
    g_api.registerEndEmitterFeed(&EndEmitterFeed);

    g_api.log("[DynamicLightMod] All hooks registered");
    return true;
}

extern "C" __declspec(dllexport) void ShutdownMod()
{
    g_lights.clear();
}

