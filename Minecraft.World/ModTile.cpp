#include "stdafx.h"
#include "stdafx.h"

#include "ModTile.h"
#include "Material.h"
#include "IconRegister.h"
#include "ContentHooks.h"

// MOD_BLOCK_* flag bits (mirror of ModAPI.h -- duplicated to avoid
// pulling ModAPI.h into Minecraft.World)
#define _MOD_BLOCK_TICKING  (1 << 2)
#define _MOD_BLOCK_ENTITY   (1 << 3)

ModTile::ModTile(int       tileId,
                 Material* material,
                 const std::wstring& textureName,
                 float     hardness,
                 float     resistance,
                 int       lightEmission,
                 int       lightOpacity,
                 int       flags)
    : Tile(tileId, material)
    , m_iconName(textureName)
{
    // Apply properties via the chainable Tile setters.
    setIconName(textureName);

    if (hardness <= 0.0f)
        setIndestructible();
    else
        setDestroyTime(hardness);

    if (resistance > 0.0f)
        setExplodeable(resistance);

    // lightEmission is 0-15; Tile uses a 0.0-1.0 normalised float.
    if (lightEmission > 0)
        setLightEmission(static_cast<float>(lightEmission) / 16.0f);

    if (lightOpacity >= 0)
        setLightBlock(lightOpacity);

    _isTicking    = (flags & _MOD_BLOCK_TICKING) != 0;
    _isEntityTile = (flags & _MOD_BLOCK_ENTITY)  != 0;
}

void ModTile::registerIcons(IconRegister* iconRegister)
{
    if (iconRegister)
        icon = iconRegister->registerIcon(m_iconName);
}

wstring ModTile::getTileItemIconName()
{
    return m_iconName;
}

bool ModTile::use(Level *level, int x, int y, int z,
                  shared_ptr<Player> player,
                  int clickedFace,
                  float clickX, float clickY, float clickZ,
                  bool soundOnly)
{
    if (g_openModGuiForBlock)
    {
        bool opened = g_openModGuiForBlock(id,
            static_cast<void*>(player.get()),
            static_cast<void*>(level),
            x, y, z);
        if (opened)
            return true;
    }

    if (g_modBlockOnUse)
    {
        bool handled = g_modBlockOnUse(id,
            static_cast<void*>(level), x, y, z,
            static_cast<void*>(player.get()),
            clickedFace, clickX, clickY, clickZ);
        if (handled)
            return true;
    }
    return Tile::use(level, x, y, z, player, clickedFace, clickX, clickY, clickZ, soundOnly);
}

void ModTile::tick(Level *level, int x, int y, int z, Random *random)
{
    if (g_modBlockOnTick)
        g_modBlockOnTick(id, static_cast<void*>(level), x, y, z, static_cast<void*>(random));
    Tile::tick(level, x, y, z, random);
}

void ModTile::onPlace(Level *level, int x, int y, int z)
{
    if (g_modBlockOnPlaced)
        g_modBlockOnPlaced(id, static_cast<void*>(level), x, y, z, nullptr, 0);
    Tile::onPlace(level, x, y, z);
}

void ModTile::neighborChanged(Level *level, int x, int y, int z, int type)
{
    if (g_modBlockOnNeighborChanged)
        g_modBlockOnNeighborChanged(id, static_cast<void*>(level), x, y, z, type);
    Tile::neighborChanged(level, x, y, z, type);
}

void ModTile::playerWillDestroy(Level *level, int x, int y, int z, int data,
                                shared_ptr<Player> player)
{
    if (g_modBlockOnBreak)
        g_modBlockOnBreak(id, static_cast<void*>(level), x, y, z, static_cast<void*>(player.get()));
    Tile::playerWillDestroy(level, x, y, z, data, player);
}
