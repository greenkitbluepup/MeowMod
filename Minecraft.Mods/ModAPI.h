#pragma once

// ModAPI.h - Shared ABI between the engine and mod DLLs.
// Keep this header self-contained so mod authors can include it without
// pulling in any engine headers.

#define MOD_API_VERSION 5

// Called once per game tick on the game thread.
using ClientTickFn = void (*)(void);

// Called by the engine for every brightness/light-color query on the
// client level. Return the dynamic block-light contribution (0-15) at
// block position (x, y, z), or 0 for no contribution here.
// The engine takes the max of the static baked value and your return value.
// Called very frequently every frame -- keep this fast.
using QueryDynamicLightFn   = int  (*)(int x, int y, int z);

// --- Chunk bake snapshot API ---
//
// These three callbacks let your mod inject dynamic light into terrain
// chunk bakes without touching live mutable state from the bake thread.
//
// Sequence per chunk rebuild:
//   1. PrepareChunkLightSnapshot  -- game thread, before dispatch
//   2. QueryChunkSnapshotLight    -- bake thread, during rebuild (read-only)
//   3. DestroyChunkLightSnapshot  -- game thread, after rebuild completes
//
// If PrepareChunkLightSnapshot returns nullptr the engine skips the wrap
// entirely -- zero overhead for chunks with no nearby emitters.
//
// Game thread. Allocate and return a frozen snapshot for this chunk, or
// nullptr to skip. chunkX/Y/Z are chunk-space (block >> 4).
using PrepareChunkLightSnapshotFn = void* (*)(int chunkX, int chunkY, int chunkZ);

// Bake thread. Query light from the frozen snapshot at block position
// (x, y, z). Must not touch any live mutable mod state. Return 0-15.
using QueryChunkSnapshotLightFn   = int   (*)(void* snapshot, int x, int y, int z);

// Game thread. Free the snapshot. Called on all paths -- always paired
// with a non-null PrepareChunkLightSnapshot return value.
using DestroyChunkLightSnapshotFn = void  (*)(void* snapshot);

// --- Emitter feed API ---
//
// The engine calls these inside Level::tickEntities(), client-side only,
// in strict order once per tick:
//
//   BeginEmitterFeed  -- start of feed; mark all known emitters unseen
//   NotifyEmitter x N -- one call per entity with non-zero emission
//   EndEmitterFeed    -- end of feed; remove unseen, rebuild fields
//
// entityId : stable per-entity integer identity
// x, y, z  : floored world-block position of the entity this tick
// strength : block-light emission level, 1-15
//
// EndEmitterFeed is the correct place to run BFS and call markRegionDirty.
// Register all three or none.
using BeginEmitterFeedFn = void (*)(void);
using NotifyEmitterFn    = void (*)(int entityId, int x, int y, int z, int strength);
using EndEmitterFeedFn   = void (*)(void);

// Passed to InitMod. Store a copy of the whole struct -- do not keep
// a pointer to the original.
struct ModHostAPI
{
    // Check this first in InitMod. Abort if != MOD_API_VERSION.
    int  apiVersion;

    // Write a message to the engine debug output (OutputDebugStringA).
    void (*log)(const char* message);

    // --- Register callbacks (call during InitMod only) ---

    // Called once per game tick on the game thread.
    void (*registerClientTick)(ClientTickFn fn);

    // Runtime per-frame brightness query. Called extremely frequently.
    void (*registerDynamicLightQuery)(QueryDynamicLightFn fn);

    // Chunk bake snapshot pipeline. Register all three or none.
    void (*registerPrepareChunkLightSnapshot)(PrepareChunkLightSnapshotFn fn);
    void (*registerQueryChunkSnapshotLight)(QueryChunkSnapshotLightFn fn);
    void (*registerDestroyChunkLightSnapshot)(DestroyChunkLightSnapshotFn fn);

    // Emitter feed pipeline. Register all three or none.
    void (*registerBeginEmitterFeed)(BeginEmitterFeedFn fn);
    void (*registerNotifyEmitter)(NotifyEmitterFn fn);
    void (*registerEndEmitterFeed)(EndEmitterFeedFn fn);

    // --- Host-provided queries (call on the game thread at any time) ---

    // Returns Tile::lightBlock[getTile(x,y,z)].
    // 0 = fully transparent, 15 = fully opaque.
    // Use this during BFS propagation in EndEmitterFeed.
    int (*getTileOpacity)(int x, int y, int z);

    // Mark a block-coordinate AABB dirty for chunk re-bake.
    // Call from EndEmitterFeed when emitters move so nearby terrain
    // is re-baked with a fresh snapshot on the next rebuild cycle.
    // Coordinates are inclusive block-space bounds.
    void (*markRegionDirty)(int x0, int y0, int z0, int x1, int y1, int z1);
};

// Required exports -- every mod DLL must define both with C linkage.
// Example:
//   extern "C" __declspec(dllexport) bool InitMod(ModHostAPI* api) { ... }
//   extern "C" __declspec(dllexport) void ShutdownMod() { ... }
using InitModFn     = bool (*)(ModHostAPI* api);
using ShutdownModFn = void (*)(void);
