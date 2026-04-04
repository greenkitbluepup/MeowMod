#include "stdafx.h"
#include "ModLightRegion.h"
#include "..\Minecraft.World\ChunkBakeHooks.h"
#include "..\Minecraft.World\Level.h"

ModLightRegion::ModLightRegion(LevelSource* inner, void* snapshot)
    : m_inner(inner), m_snapshot(snapshot)
{
}

ModLightRegion::~ModLightRegion()
{
    delete m_inner;
}

// ---------------------------------------------------------------------------
// Internal helper
// ---------------------------------------------------------------------------

int ModLightRegion::dynAt(int x, int y, int z) const
{
    if (!g_queryChunkSnapshotLight || !m_snapshot)
        return 0;
    int v = g_queryChunkSnapshotLight(m_snapshot, x, y, z);
    if (v < 0)  v = 0;
    if (v > 15) v = 15;
    return v;
}

// ---------------------------------------------------------------------------
// Light query overrides
// ---------------------------------------------------------------------------

int ModLightRegion::getLightColor(int x, int y, int z, int emitt, int tileId)
{
    int packed = m_inner->getLightColor(x, y, z, emitt, tileId);
    int dyn = dynAt(x, y, z);
    if (dyn == 0)
        return packed;

    // packed = (sky << 20) | (block << 4)
    // Only raise the block channel; sky is untouched.
    int block = (packed >> 4) & 0xF;
    if (dyn > block)
    {
        packed = (packed & ~(0xF << 4)) | (dyn << 4);
    }
    return packed;
}

float ModLightRegion::getBrightness(int x, int y, int z, int emitt)
{
    int dyn = dynAt(x, y, z);
    if (dyn > emitt)
        emitt = dyn;
    return m_inner->getBrightness(x, y, z, emitt);
}

float ModLightRegion::getBrightness(int x, int y, int z)
{
    // Delegate through the emitt overload so the dynamic floor is applied.
    int dyn = dynAt(x, y, z);
    return m_inner->getBrightness(x, y, z, dyn);
}

int ModLightRegion::getBrightness(LightLayer::variety layer, int x, int y, int z)
{
    int v = m_inner->getBrightness(layer, x, y, z);
    if (layer == LightLayer::Block)
    {
        int dyn = dynAt(x, y, z);
        if (dyn > v) v = dyn;
    }
    return v;
}

// ---------------------------------------------------------------------------
// Pass-through overrides
// ---------------------------------------------------------------------------

int ModLightRegion::getTile(int x, int y, int z)
{
    return m_inner->getTile(x, y, z);
}

shared_ptr<TileEntity> ModLightRegion::getTileEntity(int x, int y, int z)
{
    return m_inner->getTileEntity(x, y, z);
}

int ModLightRegion::getData(int x, int y, int z)
{
    return m_inner->getData(x, y, z);
}

Material* ModLightRegion::getMaterial(int x, int y, int z)
{
    return m_inner->getMaterial(x, y, z);
}

bool ModLightRegion::isSolidRenderTile(int x, int y, int z)
{
    return m_inner->isSolidRenderTile(x, y, z);
}

bool ModLightRegion::isSolidBlockingTile(int x, int y, int z)
{
    return m_inner->isSolidBlockingTile(x, y, z);
}

bool ModLightRegion::isEmptyTile(int x, int y, int z)
{
    return m_inner->isEmptyTile(x, y, z);
}

Biome* ModLightRegion::getBiome(int x, int z)
{
    return m_inner->getBiome(x, z);
}

BiomeSource* ModLightRegion::getBiomeSource()
{
    return m_inner->getBiomeSource();
}

int ModLightRegion::getMaxBuildHeight()
{
    return m_inner->getMaxBuildHeight();
}

bool ModLightRegion::isAllEmpty()
{
    return m_inner->isAllEmpty();
}

bool ModLightRegion::isTopSolidBlocking(int x, int y, int z)
{
    return m_inner->isTopSolidBlocking(x, y, z);
}

int ModLightRegion::getDirectSignal(int x, int y, int z, int dir)
{
    return m_inner->getDirectSignal(x, y, z, dir);
}
