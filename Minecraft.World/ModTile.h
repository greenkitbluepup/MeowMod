#pragma once
#pragma once

#include "Tile.h"
#include <string>

// ModTile -- a concrete Tile subclass instantiated at runtime for each
// block registered via ModHostAPI::registerBlock.
//
// Construction registers the tile in Tile::tiles[id] (via the Tile base
// constructor) and applies hardness, resistance, light, and icon name.
// One instance is allocated per registered mod block and intentionally
// never deleted (same lifetime as vanilla tiles).
class ModTile : public Tile
{
public:
    ModTile(int      tileId,
            Material* material,
            const std::wstring& textureName,
            float    hardness,
            float    resistance,
            int      lightEmission,  // 0-15
            int      lightOpacity,   // 0-15
            int      flags);         // MOD_BLOCK_* bitmask

    virtual void    registerIcons(IconRegister* iconRegister) override;
    virtual wstring getTileItemIconName() override;
    virtual bool use(Level *level, int x, int y, int z,
                     shared_ptr<Player> player,
                     int clickedFace,
                     float clickX, float clickY, float clickZ,
                     bool soundOnly = false) override;
    virtual void tick(Level *level, int x, int y, int z, Random *random) override;
    virtual void onPlace(Level *level, int x, int y, int z) override;
    virtual void neighborChanged(Level *level, int x, int y, int z, int type) override;
    virtual void playerWillDestroy(Level *level, int x, int y, int z, int data,
                                   shared_ptr<Player> player) override;

private:
    std::wstring m_iconName; // stored for getTileItemIconName()
};
