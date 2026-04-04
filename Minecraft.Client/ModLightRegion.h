#pragma once

#include "..\Minecraft.World\LevelSource.h"

// ModLightRegion wraps a Region (which it owns) and injects mod-supplied
// dynamic lighting into the four light query methods that TileRenderer uses
// during chunk baking.
//
// The snapshot handle was prepared on the game thread; this object is only
// ever read by the bake thread. It must not call any live mod state.

class ModLightRegion : public LevelSource
{
public:
    // Takes ownership of inner (will delete it in destructor).
    // snapshot is the opaque handle returned by g_prepareChunkLightSnapshot.
    ModLightRegion(LevelSource* inner, void* snapshot);
    ~ModLightRegion() override;

    // Light query overrides — merge static + mod snapshot contribution.
    int   getLightColor(int x, int y, int z, int emitt, int tileId = -1) override;
    float getBrightness(int x, int y, int z, int emitt) override;
    float getBrightness(int x, int y, int z) override;
    int   getBrightness(LightLayer::variety layer, int x, int y, int z) override;

    // All other LevelSource methods forward straight to the inner region.
    int    getTile(int x, int y, int z) override;
    shared_ptr<TileEntity> getTileEntity(int x, int y, int z) override;
    int    getData(int x, int y, int z) override;
    Material* getMaterial(int x, int y, int z) override;
    bool   isSolidRenderTile(int x, int y, int z) override;
    bool   isSolidBlockingTile(int x, int y, int z) override;
    bool   isEmptyTile(int x, int y, int z) override;
    Biome* getBiome(int x, int z) override;
    BiomeSource* getBiomeSource() override;
    int    getMaxBuildHeight() override;
    bool   isAllEmpty() override;
    bool   isTopSolidBlocking(int x, int y, int z) override;
    int    getDirectSignal(int x, int y, int z, int dir) override;

private:
    LevelSource* m_inner;
    void*        m_snapshot;

    // Returns the dynamic contribution (0-15) from the snapshot at (x,y,z).
    int dynAt(int x, int y, int z) const;
};
