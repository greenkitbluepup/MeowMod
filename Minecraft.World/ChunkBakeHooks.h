#pragma once

// ChunkBakeHooks.h - Engine-side hook slots for the chunk bake pipeline.
//
// Zero dependency on Minecraft.Mods. ModLoader writes these slots on the
// game thread after loading mods. Chunk rebuild code reads them.
//
// Threading contract:
//   g_prepareChunkLightSnapshot  -- called on the game thread only.
//   g_queryChunkSnapshotLight    -- called on any thread; must be read-only.
//   g_destroyChunkLightSnapshot  -- called on the game thread only, after
//                                   the bake thread has finished.

// Called on the game thread before a chunk rebuild is dispatched.
// Returns an opaque snapshot handle the mod allocates; nullptr means "no
// mod lighting for this chunk".
extern void* (*g_prepareChunkLightSnapshot)(int chunkX, int chunkY, int chunkZ);

// Called on the bake thread to query dynamic light from the frozen snapshot.
// Must not touch any live mutable mod state.
extern int   (*g_queryChunkSnapshotLight)(void* snapshot, int x, int y, int z);

// Called on the game thread after rebuild completes (all paths, including
// early-out). The mod must free whatever it allocated in prepare.
extern void  (*g_destroyChunkLightSnapshot)(void* snapshot);
