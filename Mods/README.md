# Minecraft Consoles Mod Loader

Drop a mod DLL into the `mods/` folder next to the game executable. The engine loads every `.dll` it finds there on startup. No config file, no registration step.

---

## Creating a mod

### 1. Copy the template

```
Mods/Template/
```

Rename the folder, rename `TemplateMod.cpp`, and update the target name in `CMakeLists.txt` to match.

### 2. Add it to the build

In the root `CMakeLists.txt`, inside the `if(BUILD_MODS)` block:

```cmake
add_subdirectory(Mods/YourModName)
```

### 3. Build

```
cmake --build build/windows64 --config Release
```

Your DLL will be compiled and copied to `mods/YourModName.dll` automatically.

### 4. Run

Launch the game. The loader finds and loads every `.dll` in `mods/` at startup.

---

## The API

Everything is in one header: `Minecraft.Mods/ModAPI.h`

Include it, implement two exports, done:

```cpp
#include "ModAPI.h"

extern "C" __declspec(dllexport) bool InitMod(ModHostAPI* api)
{
    if (!api || api->apiVersion != MOD_API_VERSION)
        return false;
    // register callbacks here
    return true;
}

extern "C" __declspec(dllexport) void ShutdownMod()
{
    // clean up here
}
```

---

## Hooks at a glance

| Hook | When it runs | Thread | Use for |
|---|---|---|---|
| `ClientTickFn` | Once per game tick | Game | Per-frame logic, polling state |
| `QueryDynamicLightFn` | Every brightness query | Game | Runtime dynamic lighting |
| `BeginEmitterFeedFn` | Start of entity tick | Game | Mark emitters unseen |
| `NotifyEmitterFn` | Per light-emitting entity | Game | Update emitter positions |
| `EndEmitterFeedFn` | End of entity tick | Game | Rebuild fields, mark dirty |
| `PrepareChunkLightSnapshotFn` | Before chunk rebuild | Game | Create frozen light snapshot |
| `QueryChunkSnapshotLightFn` | During chunk rebuild | **Bake** | Read frozen snapshot |
| `DestroyChunkLightSnapshotFn` | After chunk rebuild | Game | Free snapshot |

---

## Host-provided functions

These are always available after `InitMod` is called:

```cpp
api->log("message");
// Writes to the debug output window.

api->getTileOpacity(x, y, z);
// Returns Tile::lightBlock at world position. 0=transparent, 15=opaque.
// Use during BFS light propagation in EndEmitterFeed.

api->markRegionDirty(x0, y0, z0, x1, y1, z1);
// Marks a block-coordinate AABB for chunk re-bake.
// Call from EndEmitterFeed when emitters move.
```

---

## Threading rules

**Game thread:** all callbacks except `QueryChunkSnapshotLight`.

**Bake thread (read-only):** `QueryChunkSnapshotLight` only.

The snapshot passed to `QueryChunkSnapshotLight` must be fully immutable by the time `PrepareChunkLightSnapshot` returns. Never read live mod state from the bake thread.

---

## What is and isn't supported

**Supported**
- Everything in `ModAPI.h`
- Registering any combination of the above hooks
- Allocating and owning your own heap objects (snapshots, fields, maps)
- Calling `getTileOpacity` and `markRegionDirty` on the game thread

**Not supported**
- Including engine headers from your mod
- Accessing engine internals directly
- Calling any engine function not in `ModHostAPI`
- Touching live state from the bake thread
- Assuming specific memory layouts of engine types

---

## Worked example

`Mods/DynamicLights/` is a full worked example of all hooks used together:

- emitter feed to track held light sources
- BFS propagation using `getTileOpacity`
- `markRegionDirty` to trigger terrain re-bakes
- chunk snapshot for bake-thread-safe terrain lighting
- runtime query for entity/item lighting

Read `DynamicLightManager.h` and `DynamicLightManager.cpp` for the implementation.

---

## Disabling a mod

Delete or move its `.dll` from the `mods/` folder. The game runs clean with no mods present.
