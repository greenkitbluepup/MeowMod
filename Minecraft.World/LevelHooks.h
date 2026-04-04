#pragma once

// LevelHooks.h - Engine-side hook slots that Level.cpp calls through,
// and host-provided functions that mods call into the engine.
//
// Zero dependency on Minecraft.Mods. ModLoader writes/reads these slots.

// Mod ? engine: returns the dynamic block-light contribution (0-15) at (x,y,z).
// Null when no mod has registered a provider.
extern int (*g_queryDynamicLight)(int x, int y, int z);

// Engine ? mod: returns Tile::lightBlock[getTile(x,y,z)] at world position.
// Set by the client-side Level after construction; null until then.
// Used by mod BFS propagation on the game thread only.
extern int (*g_hostGetTileOpacity)(int x, int y, int z);

// Emitter feed slots -- fired from Level::tickEntities() client-side.
extern void (*g_beginEmitterFeed)(void);
extern void (*g_notifyEmitter)(int entityId, int x, int y, int z, int strength);
extern void (*g_endEmitterFeed)(void);

// Mark a block-coordinate region dirty for chunk rebuild.
// Set by LevelRenderer on construction; null until then.
// Called by the mod after emitters move to trigger terrain re-bake.
extern void (*g_markRegionDirty)(int x0, int y0, int z0, int x1, int y1, int z1);
