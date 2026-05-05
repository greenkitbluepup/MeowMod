#include "stdafx.h"
#include "stdafx.h"
#include <string>

#include "ModContentRegistry.h"
#include "ContentHooks.h"
#include "ModTile.h"
#include "ModItem.h"
#include "Material.h"
#include "Tile.h"
#include "Item.h"
#include "ItemInstance.h"
#include "Recipes.h"
#include "ShapedRecipy.h"
#include "FurnaceRecipes.h"
#include "Level.h"
#include "TileEntity.h"
#include "CommandBlockEntity.h"
#include "Entity.h"
#include "ItemEntity.h"
#include "GameRules.h"

// Local mirrors of MOD_MATERIAL_* and MOD_BLOCK_* constants.
// Duplicated here to keep Minecraft.World free of Minecraft.Mods headers.
enum { _MAT_ROCK=0,_MAT_DIRT,_MAT_WOOD,_MAT_METAL,_MAT_CLOTH,_MAT_SAND,_MAT_GLASS,_MAT_PLANT };

// ---------------------------------------------------------------------------
// Tile creation
// ---------------------------------------------------------------------------

static int HostCreateModTile(int id,
                              const wchar_t* textureName,
                              int   material,
                              float hardness,
                              float resistance,
                              int   lightEmission,
                              int   lightOpacity,
                              int   shape,
                              int   flags)
{
    if (id < 1 || id >= Tile::TILE_NUM_COUNT)
        return 0;
    if (Tile::tiles && Tile::tiles[id] != nullptr)
        return 0; // ID collision

    static const Material* matMap[] = {
        /* 0 ROCK  */ nullptr, // filled at call time
        /* 1 DIRT  */ nullptr,
        /* 2 WOOD  */ nullptr,
        /* 3 METAL */ nullptr,
        /* 4 CLOTH */ nullptr,
        /* 5 SAND  */ nullptr,
        /* 6 GLASS */ nullptr,
        /* 7 PLANT */ nullptr,
    };
    // Resolve lazily (Material statics are live by now)
    Material* mat;
    switch (material) {
        case _MAT_DIRT:  mat = Material::dirt;   break;
        case _MAT_WOOD:  mat = Material::wood;   break;
        case _MAT_METAL: mat = Material::metal;  break;
        case _MAT_CLOTH: mat = Material::cloth;  break;
        case _MAT_SAND:  mat = Material::sand;   break;
        case _MAT_GLASS: mat = Material::glass;  break;
        case _MAT_PLANT: mat = Material::plant;  break;
        default:         mat = Material::stone;  break; // MOD_MATERIAL_ROCK
    }

    // ModTile constructor registers itself in Tile::tiles[id].
    new ModTile(id, mat, std::wstring(textureName),
                hardness, resistance, lightEmission, lightOpacity, flags);
    (void)shape; // shape rendering is a future extension
    return id;
}

static void HostRegisterModTileEntity(const wchar_t* id, void* createFn)
{
    if (!id || !createFn)
        return;
    TileEntity::registerExternalFactory(id, reinterpret_cast<tileEntityCreateFn>(createFn));
}

// ---------------------------------------------------------------------------
// Item creation
// ---------------------------------------------------------------------------

static int HostCreateModItem(int id,
                              const wchar_t* textureName,
                              int maxStack,
                              int flags)
{
    // Item(n) constructor sets this->id = 256+n and items[256+n] = this.
    int ctorParam = id - 256;
    if (ctorParam < 0 || id >= Item::ITEM_NUM_COUNT)
        return 0;

    new ModItem(ctorParam, std::wstring(textureName), maxStack, flags);
    return id;
}

// ---------------------------------------------------------------------------
// Shaped crafting recipe
// ---------------------------------------------------------------------------

static void HostAddShapedRecipe(int outputId, int outputCount, int outputAux,
                                 int width, int height,
                                 const int* gridItems, const int* gridAux)
{
    if (!Recipes::getInstance()) return;
    int size = width * height;
    if (size < 1 || size > 9) return;

    ItemInstance** grid = new ItemInstance*[size];
    for (int i = 0; i < size; ++i)
    {
        if (gridItems[i] < 0)
            grid[i] = nullptr;
        else
            grid[i] = new ItemInstance(gridItems[i], 1,
                                       gridAux ? gridAux[i] : 0);
    }
    ItemInstance* result = new ItemInstance(outputId, outputCount, outputAux);
    Recipes::getInstance()->addRecipeDirect(
        new ShapedRecipy(width, height, grid, result));
}

// ---------------------------------------------------------------------------
// Shapeless crafting recipe
// ---------------------------------------------------------------------------

static void HostAddShapelessRecipe(int outputId, int outputCount, int outputAux,
                                    int count,
                                    const int* items, const int* auxVals)
{
    // Shapeless recipes are stored as 1xN shaped recipes with no position
    // constraint.  ShapedRecipy::matches already handles mirroring/shifting,
    // so a 1-row shaped recipe behaves as shapeless for single-row patterns.
    // A full shapeless implementation requires ShapelessRecipy; stub here.
    if (!Recipes::getInstance() || count < 1 || count > 9) return;

    ItemInstance** grid = new ItemInstance*[count];
    for (int i = 0; i < count; ++i)
        grid[i] = new ItemInstance(items[i], 1, auxVals ? auxVals[i] : 0);

    ItemInstance* result = new ItemInstance(outputId, outputCount, outputAux);
    Recipes::getInstance()->addRecipeDirect(
        new ShapedRecipy(count, 1, grid, result));
}

// ---------------------------------------------------------------------------
// Furnace recipe
// ---------------------------------------------------------------------------

static void HostAddFurnaceRecipe(int inputId,
                                  int outputId, int outputCount, int outputAux,
                                  float experience)
{
    if (!FurnaceRecipes::getInstance()) return;
    ItemInstance* result = new ItemInstance(outputId, outputCount, outputAux);
    FurnaceRecipes::getInstance()->addFurnaceRecipy(inputId, result, experience);
}

// ---------------------------------------------------------------------------
// Worldgen block placement
// ---------------------------------------------------------------------------

static void HostWorldGenPlaceBlock(void* levelHandle,
                                    int x, int y, int z,
                                    int tileId, int data)
{
    Level* level = static_cast<Level*>(levelHandle);
    if (level)
        level->setTileAndData(x, y, z, tileId, data,
                              Tile::UPDATE_CLIENTS | Tile::UPDATE_NEIGHBORS);
}

static bool HostResolveStructurePlacementSender(void* sender,
                                                void** outLevel,
                                                int* outX,
                                                int* outY,
                                                int* outZ)
{
    if (!sender || !outLevel || !outX || !outY || !outZ)
        return false;

    CommandBlockEntity* cmd = static_cast<CommandBlockEntity*>(sender);
    if (!cmd || !cmd->level)
        return false;

    *outLevel = static_cast<void*>(cmd->level);
    *outX = cmd->x;
    *outY = cmd->y;
    *outZ = cmd->z;
    return true;
}

static bool HostMarkModTileEntityDirty(void* tileEntity)
{
    if (!tileEntity)
        return false;

    TileEntity* te = static_cast<TileEntity*>(tileEntity);
    if (!te || !te->level)
        return false;

    te->setChanged();
    te->level->sendTileUpdated(te->x, te->y, te->z);
    return true;
}

static bool HostApplyBlockDropEntries(void* levelHandle,
                                      int x, int y, int z,
                                      const ModDropEntryHost* entries,
                                      int entryCount,
                                      int playerBonusLevel)
{
    (void)playerBonusLevel;
    Level* level = static_cast<Level*>(levelHandle);
    if (!level || !entries || entryCount <= 0)
        return false;

    bool spawnedAny = false;
    for (int i = 0; i < entryCount; ++i)
    {
        const ModDropEntryHost& d = entries[i];
        if (d.itemId <= 0)
            continue;
        float chance = d.chance;
        if (chance < 0.0f) chance = 0.0f;
        if (chance > 1.0f) chance = 1.0f;
        if (level->random->nextFloat() > chance)
            continue;

        int minCount = d.minCount < 0 ? 0 : d.minCount;
        int maxCount = d.maxCount < minCount ? minCount : d.maxCount;
        int count = minCount;
        if (maxCount > minCount)
            count += level->random->nextInt((maxCount - minCount) + 1);
        if (count <= 0)
            continue;

        if (!level->isClientSide && level->getGameRules()->getBoolean(GameRules::RULE_DOTILEDROPS))
        {
            float s = 0.7f;
            double xo = level->random->nextFloat() * s + (1 - s) * 0.5;
            double yo = level->random->nextFloat() * s + (1 - s) * 0.5;
            double zo = level->random->nextFloat() * s + (1 - s) * 0.5;
            shared_ptr<ItemEntity> item = std::make_shared<ItemEntity>(
                level,
                x + xo, y + yo, z + zo,
                std::make_shared<ItemInstance>(d.itemId, count, d.aux));
            item->throwTime = 10;
            level->addEntity(item);
        }
        spawnedAny = true;
    }

    return spawnedAny;
}

static bool HostApplyEntityDropEntries(void* entityHandle,
                                       void* levelHandle,
                                       const ModDropEntryHost* entries,
                                       int entryCount,
                                       bool wasKilledByPlayer,
                                       int playerBonusLevel)
{
    (void)levelHandle;
    (void)wasKilledByPlayer;
    (void)playerBonusLevel;
    Entity* entity = static_cast<Entity*>(entityHandle);
    if (!entity || !entries || entryCount <= 0 || !entity->level || !entity->level->random)
        return false;

    bool spawnedAny = false;
    for (int i = 0; i < entryCount; ++i)
    {
        const ModDropEntryHost& d = entries[i];
        if (d.itemId <= 0)
            continue;
        float chance = d.chance;
        if (chance < 0.0f) chance = 0.0f;
        if (chance > 1.0f) chance = 1.0f;
        if (entity->level->random->nextFloat() > chance)
            continue;

        int minCount = d.minCount < 0 ? 0 : d.minCount;
        int maxCount = d.maxCount < minCount ? minCount : d.maxCount;
        int count = minCount;
        if (maxCount > minCount)
            count += entity->level->random->nextInt((maxCount - minCount) + 1);
        if (count <= 0)
            continue;

        entity->spawnAtLocation(std::make_shared<ItemInstance>(d.itemId, count, d.aux), 0.0f);
        spawnedAny = true;
    }

    return spawnedAny;
}

// ---------------------------------------------------------------------------
// Public init -- call before Tile::staticCtor()
// ---------------------------------------------------------------------------

void initModContentRegistry()
{
    g_hostCreateModTile      = &HostCreateModTile;
    g_hostCreateModItem      = &HostCreateModItem;
    g_hostAddShapedRecipe    = &HostAddShapedRecipe;
    g_hostAddShapelessRecipe = &HostAddShapelessRecipe;
    g_hostAddFurnaceRecipe   = &HostAddFurnaceRecipe;
    g_worldGenPlaceBlock     = &HostWorldGenPlaceBlock;
    g_hostResolveStructurePlacementSender = &HostResolveStructurePlacementSender;
    g_hostMarkModTileEntityDirty = &HostMarkModTileEntityDirty;
    g_hostApplyBlockDropEntries = &HostApplyBlockDropEntries;
    g_hostApplyEntityDropEntries = &HostApplyEntityDropEntries;
    g_hostRegisterModTileEntity = &HostRegisterModTileEntity;
}

namespace
{
struct AutoInitModContentRegistry
{
    AutoInitModContentRegistry() { initModContentRegistry(); }
};

AutoInitModContentRegistry g_autoInitModContentRegistry;
}
