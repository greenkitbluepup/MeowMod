#pragma once
#pragma once

// DynamicLightManager.h
//
// Self-contained dynamic lighting engine for the mod DLL.
// No engine headers — only the types from ModAPI.h are needed externally.
//
// Threading contract (matches engine invariant):
//   updateEmitter / removeUnseen / rebuildFields / getDynamicContribution
//     -> game thread only (inside emitter feed callbacks)
//   Snapshot objects returned by takeSnapshot()
//     -> immutable after creation; safe to read from any thread

#include <cstdint>
#include <vector>
#include <unordered_map>

// Function pointer type matching getTileOpacity in ModHostAPI.
using GetTileOpacityFn    = int  (*)(int x, int y, int z);
using MarkRegionDirtyFn   = void (*)(int x0, int y0, int z0, int x1, int y1, int z1);

// ---------------------------------------------------------------------------
// BFS field for one emitter
// ---------------------------------------------------------------------------

struct DynamicField
{
    int x = 0, y = 0, z = 0;   // emitter block position
    int strength = 0;           // 0-15
    bool fieldDirty = true;
    bool built = false;

    // (2*strength+1)^3 values, 0-15
    std::vector<uint8_t> cells;

    void rebuild(GetTileOpacityFn getTileOpacity);
    int  getContribution(int wx, int wy, int wz) const;
};

// ---------------------------------------------------------------------------
// Snapshot: owned frozen copy of all active fields at one instant
// ---------------------------------------------------------------------------

struct FieldSnapshot
{
    int x, y, z, strength;
    std::vector<uint8_t> cells;

    int getContribution(int wx, int wy, int wz) const;
};

struct DynamicLightSnapshot
{
    std::vector<FieldSnapshot> fields;

    int getDynamicContribution(int wx, int wy, int wz) const;
};

// ---------------------------------------------------------------------------
// Per-emitter entry
// ---------------------------------------------------------------------------

struct EmitterEntry
{
    int  entityId = 0;
    int  x = 0, y = 0, z = 0;
    int  strength = 0;
    bool seenThisFeed = false;
    DynamicField field;
};

// ---------------------------------------------------------------------------
// Manager
// ---------------------------------------------------------------------------

class DynamicLightManager
{
public:
    void setTileOpacityFn(GetTileOpacityFn fn)   { m_getTileOpacity   = fn; }
    void setMarkRegionDirtyFn(MarkRegionDirtyFn fn) { m_markRegionDirty = fn; }

    // Emitter feed (game thread)
    void beginFeed();
    void notifyEmitter(int entityId, int x, int y, int z, int strength);
    void endFeed();   // removes unseen, rebuilds dirty fields

    // Runtime query (game thread, after endFeed)
    int  getDynamicContribution(int wx, int wy, int wz) const;

    // Called when world opacity may change at a block position
    // (e.g. block break/place). Marks overlapping emitter fields stale.
    void notifyBlockChanged(int x, int y, int z);

    // Snapshot for one chunk bake slot (game thread, returns heap object)
    // chunkX/Y/Z are chunk coordinates (block >> 4).
    // Returns nullptr if no emitter is near enough to affect that chunk.
    DynamicLightSnapshot* takeSnapshot(int chunkX, int chunkY, int chunkZ) const;

    void clear();

private:
    GetTileOpacityFn  m_getTileOpacity  = nullptr;
    MarkRegionDirtyFn m_markRegionDirty = nullptr;

    // Union of all field AABBs that changed this feed — flushed in endFeed.
    bool m_dirtyPending  = false;
    int  m_dirtyX0 = 0, m_dirtyY0 = 0, m_dirtyZ0 = 0;
    int  m_dirtyX1 = 0, m_dirtyY1 = 0, m_dirtyZ1 = 0;

    void expandDirty(int x, int y, int z, int r);
    void rebuildField(EmitterEntry& entry);

    std::unordered_map<int, EmitterEntry> m_entries; // keyed by entityId
};
