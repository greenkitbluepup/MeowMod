#include "DynamicLightManager.h"

#include <algorithm>
#include <queue>
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
// DynamicField
// ---------------------------------------------------------------------------

void DynamicField::rebuild(GetTileOpacityFn getTileOpacity)
{
    built = false;
    fieldDirty = false;

    if (strength <= 0)
        return;

    const int r = strength;
    const int side = 2 * r + 1;
    const int vol = side * side * side;

    cells.assign(vol, 0);

    auto idx = [&](int dx, int dy, int dz) -> int {
        return (dx + r) + (dy + r) * side + (dz + r) * side * side;
        };

    cells[idx(0, 0, 0)] = static_cast<uint8_t>(strength);

    struct Cell
    {
        int8_t dx;
        int8_t dy;
        int8_t dz;
        uint8_t val;
    };

    std::queue<Cell> q;
    q.push({ 0, 0, 0, static_cast<uint8_t>(strength) });

    static const int8_t dirs[6][3] = {
        {  1,  0,  0 },
        { -1,  0,  0 },
        {  0,  1,  0 },
        {  0, -1,  0 },
        {  0,  0,  1 },
        {  0,  0, -1 }
    };

    while (!q.empty())
    {
        Cell c = q.front();
        q.pop();

        if (c.val == 0)
            continue;

        for (auto& d : dirs)
        {
            const int ndx = c.dx + d[0];
            const int ndy = c.dy + d[1];
            const int ndz = c.dz + d[2];

            if (ndx < -r || ndx > r) continue;
            if (ndy < -r || ndy > r) continue;
            if (ndz < -r || ndz > r) continue;

            const int ni = idx(ndx, ndy, ndz);

            int opacity = 1;

            if (getTileOpacity)
            {
                const int o = getTileOpacity(x + ndx, y + ndy, z + ndz);

                if (o > opacity)
                    opacity = o;

                if (o >= 15)
                    continue;
            }

            const uint8_t newVal =
                (c.val > static_cast<uint8_t>(opacity))
                ? static_cast<uint8_t>(c.val - static_cast<uint8_t>(opacity))
                : uint8_t(0);

            if (newVal > cells[ni])
            {
                cells[ni] = newVal;

                q.push({
                    static_cast<int8_t>(ndx),
                    static_cast<int8_t>(ndy),
                    static_cast<int8_t>(ndz),
                    newVal
                    });
            }
        }
    }

    built = true;
}

int DynamicField::getContribution(int wx, int wy, int wz) const
{
    if (!built || strength <= 0)
        return 0;

    const int r = strength;

    const int dx = wx - x;
    const int dy = wy - y;
    const int dz = wz - z;

    if (dx < -r || dx > r) return 0;
    if (dy < -r || dy > r) return 0;
    if (dz < -r || dz > r) return 0;

    const int side = 2 * r + 1;
    const int i = (dx + r) + (dy + r) * side + (dz + r) * side * side;

    return cells[i];
}

// ---------------------------------------------------------------------------
// FieldSnapshot
// ---------------------------------------------------------------------------

int FieldSnapshot::getContribution(int wx, int wy, int wz) const
{
    if (strength <= 0 || cells.empty())
        return 0;

    const int r = strength;

    const int dx = wx - x;
    const int dy = wy - y;
    const int dz = wz - z;

    if (dx < -r || dx > r) return 0;
    if (dy < -r || dy > r) return 0;
    if (dz < -r || dz > r) return 0;

    const int side = 2 * r + 1;
    const int i = (dx + r) + (dy + r) * side + (dz + r) * side * side;

    return cells[i];
}

// ---------------------------------------------------------------------------
// DynamicLightSnapshot
// ---------------------------------------------------------------------------

int DynamicLightSnapshot::getDynamicContribution(int wx, int wy, int wz) const
{
    int best = 0;

    for (auto& f : fields)
    {
        const int v = f.getContribution(wx, wy, wz);

        if (v > best)
            best = v;
    }

    return best;
}

// ---------------------------------------------------------------------------
// DynamicLightManager
// ---------------------------------------------------------------------------

void DynamicLightManager::beginFeed()
{
    for (auto& kv : m_entries)
        kv.second.seenThisFeed = false;
}

void DynamicLightManager::notifyEmitter(int entityId, int x, int y, int z, int strength)
{
    auto it = m_entries.find(entityId);

    if (it == m_entries.end())
    {
        EmitterEntry e;

        e.entityId = entityId;
        e.x = x;
        e.y = y;
        e.z = z;
        e.strength = strength;
        e.seenThisFeed = true;

        e.field.x = x;
        e.field.y = y;
        e.field.z = z;
        e.field.strength = strength;
        e.field.fieldDirty = true;

        expandDirty(x, y, z, strength);

        m_entries.emplace(entityId, std::move(e));
        return;
    }

    EmitterEntry& e = it->second;
    e.seenThisFeed = true;

    if (e.x == x && e.y == y && e.z == z && e.strength == strength)
        return;

    expandDirty(e.x, e.y, e.z, e.strength);
    expandDirty(x, y, z, strength);

    e.x = x;
    e.y = y;
    e.z = z;
    e.strength = strength;

    e.field.x = x;
    e.field.y = y;
    e.field.z = z;
    e.field.strength = strength;
    e.field.fieldDirty = true;
}

void DynamicLightManager::endFeed()
{
    for (auto it = m_entries.begin(); it != m_entries.end(); )
    {
        if (!it->second.seenThisFeed)
        {
            expandDirty(
                it->second.x,
                it->second.y,
                it->second.z,
                it->second.strength
            );

            it = m_entries.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto& kv : m_entries)
    {
        EmitterEntry& e = kv.second;

        if (e.field.fieldDirty)
            rebuildField(e);
    }

    if (m_dirtyPending && m_markRegionDirty)
    {
        m_markRegionDirty(
            m_dirtyX0,
            m_dirtyY0,
            m_dirtyZ0,
            m_dirtyX1,
            m_dirtyY1,
            m_dirtyZ1
        );
    }

    m_dirtyPending = false;
}

void DynamicLightManager::rebuildField(EmitterEntry& entry)
{
    entry.field.rebuild(m_getTileOpacity);
}

int DynamicLightManager::getDynamicContribution(int wx, int wy, int wz) const
{
    int best = 0;

    for (auto& kv : m_entries)
    {
        const int v = kv.second.field.getContribution(wx, wy, wz);

        if (v > best)
            best = v;
    }

    return best;
}

void DynamicLightManager::notifyBlockChanged(int x, int y, int z)
{
    for (auto& kv : m_entries)
    {
        EmitterEntry& e = kv.second;

        if (e.strength <= 0)
            continue;

        const int r = e.strength;

        if (x < e.x - r || x > e.x + r) continue;
        if (y < e.y - r || y > e.y + r) continue;
        if (z < e.z - r || z > e.z + r) continue;

        e.field.fieldDirty = true;

        expandDirty(e.x, e.y, e.z, r);
    }
}

DynamicLightSnapshot* DynamicLightManager::takeSnapshot(int chunkX, int chunkY, int chunkZ) const
{
    const int margin = 15;

    const int wMinX = chunkX * 16 - margin;
    const int wMinY = chunkY * 16 - margin;
    const int wMinZ = chunkZ * 16 - margin;

    const int wMaxX = chunkX * 16 + 16 + margin;
    const int wMaxY = chunkY * 16 + 16 + margin;
    const int wMaxZ = chunkZ * 16 + 16 + margin;

    DynamicLightSnapshot* snap = nullptr;

    for (auto& kv : m_entries)
    {
        const EmitterEntry& e = kv.second;

        if (!e.field.built || e.field.strength <= 0)
            continue;

        const int r = e.field.strength;

        if (e.x + r < wMinX || e.x - r > wMaxX) continue;
        if (e.y + r < wMinY || e.y - r > wMaxY) continue;
        if (e.z + r < wMinZ || e.z - r > wMaxZ) continue;

        if (!snap)
            snap = new DynamicLightSnapshot();

        FieldSnapshot fs;

        fs.x = e.field.x;
        fs.y = e.field.y;
        fs.z = e.field.z;
        fs.strength = e.field.strength;
        fs.cells = e.field.cells;

        snap->fields.push_back(std::move(fs));
    }

    return snap;
}

void DynamicLightManager::clear()
{
    m_entries.clear();
    m_dirtyPending = false;
}

void DynamicLightManager::expandDirty(int x, int y, int z, int r)
{
    if (!m_dirtyPending)
    {
        m_dirtyX0 = x - r;
        m_dirtyY0 = y - r;
        m_dirtyZ0 = z - r;

        m_dirtyX1 = x + r;
        m_dirtyY1 = y + r;
        m_dirtyZ1 = z + r;

        m_dirtyPending = true;
        return;
    }

    if (x - r < m_dirtyX0) m_dirtyX0 = x - r;
    if (y - r < m_dirtyY0) m_dirtyY0 = y - r;
    if (z - r < m_dirtyZ0) m_dirtyZ0 = z - r;

    if (x + r > m_dirtyX1) m_dirtyX1 = x + r;
    if (y + r > m_dirtyY1) m_dirtyY1 = y + r;
    if (z + r > m_dirtyZ1) m_dirtyZ1 = z + r;
}