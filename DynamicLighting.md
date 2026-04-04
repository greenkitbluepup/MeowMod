# Dynamic Lighting

This document covers the design, implementation, data flow, and threading model of the dynamic lighting system. Read this before touching any of the files listed below.

---

## Overview

Dynamic lighting makes entities that hold or are near light-emitting items (torches, glowstone, fire, etc.) illuminate the world around them at runtime, rather than only affecting light when a block is placed. It runs on top of the existing static chunk light storage without modifying it.

**Affected files:**

| File | Role |
|---|---|
| `Minecraft.World/DynamicLightManager.h/.cpp` | Core manager: emitter registry, BFS field build, snapshot |
| `Minecraft.World/DynamicLightRegion.h/.cpp` | Decorator wrapping `Region` for chunk bakes |
| `Minecraft.World/DynamicLightHelpers.h` | Named wrappers for future render call sites |
| `Minecraft.World/Level.cpp` | Injection into `getLightColor`/`getBrightness`; tick-time update |
| `Minecraft.Client/Chunk.cpp` | Snapshot preparation before bake thread kickoff |

---

## Architecture: Two Consumption Paths

Dynamic light reaches the screen via two completely separate, non-overlapping paths. **Never mix them.**

```
Game thread                         Bake thread
?????????????????????????????????   ??????????????????????????????????????
Level::tickEntities()               Chunk::rebuild()
  updateEntityLight() x N             DynamicLightRegion::getLightColor()
  rebuildFields()                       ?? snapshot.getDynamicContribution()
  consumeDirtyRegion()                  (read-only, no locks needed)
  ?????????????????????
  Entity render / TileEntity render
  Level::getLightColor()
    ?? getDynamicContribution()  ? live manager, game thread only
  Level::getBrightness()
    ?? getDynamicContribution()  ? live manager, game thread only
```

### Path 1 — Runtime (game thread)

Every tick, `Level::tickEntities()` does the following inside its `isClientSide` block:

1. Iterates all entities and computes their emission value:
   - Entities on fire ? 15
   - Players ? max(held item emission, armor emission)
   - Mobs ? held item emission
2. Calls `m_dynamicLights->updateEntityLight(entity, x, y, z, emission)` for each.  
   The no-op guard in `updateEntityLight` skips dirty-marking when position and strength are unchanged, so a stationary entity costs one comparison per tick.
3. Calls `m_dynamicLights->rebuildFields(this)` to build BFS fields for any stale emitter.
4. Calls `consumeDirtyRegion()` and marks affected chunks dirty for bake.

After this, for the rest of the frame, every call to `Level::getLightColor()` or `Level::getBrightness()` on the client level automatically calls `m_dynamicLights->getDynamicContribution(x, y, z)` and takes the max over the static value. This covers:

- Entity rendering (via `Entity::getLightColor`)
- Tile entity rendering (via `TileEntityRenderDispatcher::render`)
- Item rendering
- Particle rendering
- Any future renderer that queries `Level` directly

### Path 2 — Chunk bake (rebuild thread)

Before kicking off a chunk rebuild, `Chunk::prepareDynamicLightSnapshot()` runs **on the game thread**:

1. Checks `hasLightsNear()` — if no emitter is within range, leaves `pendingSnapshot` empty and skips the rest.
2. Calls `rebuildFields(level)` to ensure all fields are fresh.
3. Calls `takeSnapshot()` to produce a frozen `DynamicLightSnapshot` — a fully owned `std::vector` copy of every active BFS field.
4. Stores it in `Chunk::pendingSnapshot`.

When `Chunk::rebuild()` runs (possibly on a separate thread), it checks `pendingSnapshot.fields.empty()`. If non-empty, it wraps the `Region` in a `DynamicLightRegion` that injects dynamic contribution from the snapshot into all `getLightColor`/`getBrightness` calls that `TileRenderer` makes during the bake. If empty, the plain `Region` is used with no overhead.

The rebuild thread **never** touches the live `DynamicLightManager`.

---

## BFS Field Format

Each registered emitter gets a `DynamicField` — a `(2r+1)³` cube of `uint8_t` values centred on the emitter, where `r = strength` (0–15). The cube is indexed as:

```
index = (dx + r) + (dy + r) * side + (dz + r) * side * side
side  = 2 * r + 1
```

Values are 0–15. The emitter cell starts at its full strength value. BFS propagation costs `max(1, Tile::lightBlock[id])` per step, identical to how vanilla static block light propagates. Fully opaque blocks (lightBlock ? 15) absorb all light and do not propagate further.

Fields are only rebuilt when `fieldDirty == true`. The no-op guard in `updateLight`/`updateEntityLight` suppresses `fieldDirty = true` when position and strength haven't changed, so stationary emitters never trigger a BFS rebuild.

---

## `DynamicLightManager` API Summary

```cpp
// Source management
int  addLight(x, y, z, strength)                  // static emitter; returns handle
int  addEntityLight(entityKey, x, y, z, strength) // entity emitter; returns handle
void updateLight(handle, x, y, z, strength)        // no-op if position+strength unchanged
void updateEntityLight(entityKey, x, y, z, strength)
void removeLight(handle)
void removeEntityLight(entityKey)
void clear()

// Field rebuild — call before any getDynamicContribution() query
void rebuildFields(LevelSource *tiles)

// Query — safe to call after rebuildFields()
int  getDynamicContribution(x, y, z)    // returns 0–15
bool hasLightsNear(cx, cy, cz, r)       // broad-phase check for chunk bake skip

// Dirty region — consumed by Level::tickEntities() to mark chunks for rebuild
bool consumeDirtyRegion(outMin[3], outMax[3])

// Snapshot — game thread only, immediately after rebuildFields()
DynamicLightSnapshot takeSnapshot()

// Diagnostics
DebugStats getDebugStats()  // resets interval counters on each call
```

---

## `DynamicLightSnapshot` / `DynamicLightRegion`

`DynamicLightSnapshot` is a plain value type (moveable, copyable) containing a `std::vector<FieldSnapshot>`. Each `FieldSnapshot` is a fully owned copy of one emitter's BFS field at the moment `takeSnapshot()` was called. It is completely independent of the live manager after construction.

`DynamicLightRegion` is a decorator implementing `LevelSource`. It wraps a `Region` (which it owns and deletes) and overrides the four light query methods:

- `getLightColor` — takes max of static block channel and `snapshot.getDynamicContribution()`
- `getBrightness(x,y,z,emitt)` — promotes emitt floor with dynamic level
- `getBrightness(x,y,z)` — falls through to emitt variant if dynamic > 0
- `getBrightness(LightLayer, x,y,z)` — dynamic injection on Block layer only; Sky is untouched

All other `LevelSource` methods forward directly to the inner `Region`.

---

## `Level` Integration

`Level::getLightColor` and both `Level::getBrightness` overloads inject dynamic contribution behind an `isClientSide` guard:

```cpp
// In Level::getLightColor:
if (isClientSide) {
    int dyn = m_dynamicLights->getDynamicContribution(x, y, z);
    if (dyn > b) b = dyn;
}

// In Level::getBrightness:
if (isClientSide) {
    int dyn = m_dynamicLights->getDynamicContribution(x, y, z);
    if (dyn > n) n = dyn;
}
```

- Server-side levels pay zero cost — `isClientSide` is false.
- `getDynamicContribution` iterates `m_entries` and skips entries where `!field.built`, so even on the client, non-emitting areas cost only the loop overhead.
- `MAX_BRIGHTNESS` (15) is enforced after injection so no value can exceed the lightmap table bounds.

---

## Directional Face Shading — Intentionally Disabled

The classic directional scalars (bottom face ×0.5, N/S ×0.8, E/W ×0.6) have been removed from both `tesselateBlockInWorldWithAmbienceOcclusionTexLighting` and `tesselateBlockInWorld`.

In `TEXTURE_LIGHTING` mode, per-face brightness comes entirely from the packed lightmap value (`sky << 20 | block << 4`) written into the vertex texture coordinate. The old scalars were designed for a fixed-function pipeline without a lightmap. Keeping them caused held-torch vs placed-torch brightness mismatches and did not match the desired in-game appearance.

This is a permanent design decision, not a temporary diagnostic removal.

---

## Adding a New Dynamic Light Source

If you need to register a light source that isn't an entity (e.g. a moving tile entity, a vehicle, a scripted effect):

1. Hold the handle returned by `addLight()` or `addEntityLight()`.
2. Call `updateLight(handle, ...)` every tick it moves. The no-op guard makes this free when it hasn't moved.
3. Call `removeLight(handle)` when it is destroyed.

The rest — BFS rebuild, chunk dirty marking, `Level` injection — is automatic.

---

## Adding a New Renderer That Needs Dynamic Light

Any renderer that queries light through `Level::getLightColor()` or `Level::getBrightness()` on a client-side level **already gets dynamic light for free** — no changes needed.

If you have a renderer that bypasses `Level` and talks directly to `LevelSource`, `Region`, or raw chunk data, it will miss dynamic light. Use `DynamicLightHelpers.h`:

```cpp
#include "DynamicLightHelpers.h"

int   col = getMergedLightColor(level, x, y, z);
float br  = getMergedBrightness(level, x, y, z);
```

These are thin wrappers over `level->getLightColor` / `level->getBrightness` that exist as named, intention-revealing call sites.

---

## `DebugStats`

`DynamicLightManager::getDebugStats()` returns a `DebugStats` struct and resets the interval counters:

| Field | Meaning |
|---|---|
| `activeEmitters` | Live count of currently active sources (always current) |
| `fieldsRebuilt` | BFS fields rebuilt since last `getDebugStats()` call |
| `bfsCellsVisited` | BFS queue-pops since last call |
| `chunksDirtied` | Dirty regions consumed since last call |

Call once per second (or per frame during profiling) and log or display the result.

---

## Threading Invariant

> **Never query the live `DynamicLightManager` from the chunk rebuild thread.**  
> **Never call `rebuildFields()` from the bake thread.**  
> The rebuild thread receives a frozen `DynamicLightSnapshot` and must use only that.

Violating this reintroduces data races between `updateEntityLight` (game thread, mutates BFS fields) and the bake thread (reads BFS fields), producing seams, flickering, and non-deterministic per-vertex lighting.
