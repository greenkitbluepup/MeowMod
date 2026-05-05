#pragma once

// ContentHooks.h - Engine-side hook slots for the mod content pipeline.
//
// Zero dependency on Minecraft.Mods. ModLoader writes the lifecycle slots
// after loadAllMods(). Engine call sites (staticCtor, Recipes ctor,
// BiomeDecorator) read and call through them.
//
// Host operation slots are set by initModContentRegistry() early in startup
// and called by ModLoader during the lifecycle callbacks.

// ---------------------------------------------------------------------------
// Mod content lifecycle (engine -> ModLoader)
// ---------------------------------------------------------------------------

// Called at the end of Tile::staticCtor() -- ModLoader creates mod tiles.
extern void (*g_registerModTiles)(void);

// Called at the end of Item::staticCtor() -- ModLoader creates mod items.
extern void (*g_registerModItems)(void);

// Called inside Recipes::Recipes() after all vanilla recipes are added.
extern void (*g_registerModRecipes)(void);

// Called at the end of TileEntity::staticCtor() -- ModLoader registers
// mod-provided tile-entity factories.
extern void (*g_registerModTileEntities)(void);

// Called from BiomeDecorator::decorate() once per chunk decoration pass.
// levelHandle : opaque Level* cast to void*
// chunkX / Z  : chunk-space coordinates (block >> 4)
// seed        : per-chunk deterministic seed
extern void (*g_decorateChunk)(void* levelHandle, int chunkX, int chunkZ,
    unsigned int seed);

// ---------------------------------------------------------------------------
// Host-provided engine operations (ModLoader -> engine)
// ---------------------------------------------------------------------------

// Create a ModTile with the given properties and insert it into
// Tile::tiles[id]. material is a MOD_MATERIAL_* constant.
// Returns id on success, 0 on failure (id out of range or already occupied).
extern int (*g_hostCreateModTile)(int id,
    const wchar_t* textureName,
    int material,
    float hardness,
    float resistance,
    int lightEmission,
    int lightOpacity,
    int shape,
    int flags);

// Create a ModItem and insert it into Item::items.
// id is the final game ID (>=256); the constructor offset is applied inside.
// Returns id on success, 0 on failure.
extern int (*g_hostCreateModItem)(int id,
    const wchar_t* textureName,
    int maxStack,
    int flags);

// Add a shaped crafting recipe.
// gridItems / gridAux are width*height arrays row-major; -1 = empty slot.
extern void (*g_hostAddShapedRecipe)(int outputId,
    int outputCount,
    int outputAux,
    int width,
    int height,
    const int* gridItems,
    const int* gridAux);

// Add a shapeless crafting recipe.
// items / auxVals are parallel arrays of length count.
extern void (*g_hostAddShapelessRecipe)(int outputId,
    int outputCount,
    int outputAux,
    int count,
    const int* items,
    const int* auxVals);

// Add a furnace (smelting) recipe.
extern void (*g_hostAddFurnaceRecipe)(int inputId,
    int outputId,
    int outputCount,
    int outputAux,
    float experience);

// Place a block during worldgen. levelHandle is the opaque Level* from
// g_decorateChunk. tileId is the numeric tile ID (mod or vanilla).
extern void (*g_worldGenPlaceBlock)(void* levelHandle,
    int x,
    int y,
    int z,
    int tileId,
    int data);

// Resolve command callback sender into a placement context.
// Returns true on success and outputs Level* plus sender block position.
extern bool (*g_hostResolveStructurePlacementSender)(void* sender,
    void** outLevel,
    int* outX,
    int* outY,
    int* outZ);

// Mod networking transport hooks.
extern bool (*g_hostSendModPacketToServer)(const wchar_t* channel,
    const void* payload,
    int size);

extern bool (*g_hostSendModPacketToClient)(void* player,
    const wchar_t* channel,
    const void* payload,
    int size);

extern int (*g_hostBroadcastModPacketToTracking)(void* level,
    int x,
    int y,
    int z,
    int radius,
    const wchar_t* channel,
    const void* payload,
    int size);

// Marks a tile entity as dirty and sends a tile update.
extern bool (*g_hostMarkModTileEntityDirty)(void* tileEntity);

// ---------------------------------------------------------------------------
// Player hand / offhand host operations (ModLoader -> engine)
// ---------------------------------------------------------------------------

struct ModItemStack;

extern void* (*g_hostGetLocalPlayer)(void);

extern bool (*g_hostGetPlayerSelectedItem)(void* player, ModItemStack* out);
extern bool (*g_hostSetPlayerSelectedItem)(void* player, const ModItemStack* stack);

extern bool (*g_hostGetPlayerOffhandItem)(void* player, ModItemStack* out);
extern bool (*g_hostSetPlayerOffhandItem)(void* player, const ModItemStack* stack);

extern bool (*g_hostSwapPlayerHands)(void* player);

// Register a tile-entity factory by string ID.
// createFn is a tileEntityCreateFn passed as an opaque pointer.
extern void (*g_hostRegisterModTileEntity)(const wchar_t* id, void* createFn);

// Execute a mod command line (e.g. from command block integration).
// Returns true if a mod command handled it.
extern bool (*g_executeModCommandText)(void* sender, const wchar_t* commandLine);

// GUI binding bridge: attempt to open a mod GUI for this block id.
// Returns true if a GUI handler was found and invoked.
extern bool (*g_openModGuiForBlock)(int blockId,
    void* player,
    void* level,
    int x,
    int y,
    int z);

// Tile-entity persistence callbacks for mod-provided tile entities.
extern void (*g_modTileEntityOnSave)(const wchar_t* id, void* tileEntity, void* tag);
extern void (*g_modTileEntityOnLoad)(const wchar_t* id, void* tileEntity, void* tag);

// ---------------------------------------------------------------------------
// Mod block interaction callback bridge (ModTile -> ModLoader)
// ---------------------------------------------------------------------------

extern bool (*g_modBlockOnUse)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    void* player,
    int clickedFace,
    float clickX,
    float clickY,
    float clickZ);

extern void (*g_modBlockOnBreak)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    void* player);

extern void (*g_modBlockOnPlaced)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    void* player,
    int itemAux);

extern void (*g_modBlockOnNeighborChanged)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    int neighborId);

extern void (*g_modBlockOnTick)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    void* random);

// Mod-driven loot/drop hooks.
// Return true when mod drops were applied and vanilla drop logic should be skipped.
extern bool (*g_modApplyBlockDrops)(int blockId,
    void* level,
    int x,
    int y,
    int z,
    int playerBonusLevel);

extern bool (*g_modApplyEntityDrops)(const wchar_t* entityId,
    void* entity,
    void* level,
    bool wasKilledByPlayer,
    int playerBonusLevel);

struct ModDropEntryHost
{
    int itemId;
    int aux;
    int minCount;
    int maxCount;
    float chance;
};

extern bool (*g_hostApplyBlockDropEntries)(void* level,
    int x,
    int y,
    int z,
    const ModDropEntryHost* entries,
    int entryCount,
    int playerBonusLevel);

extern bool (*g_hostApplyEntityDropEntries)(void* entity,
    void* level,
    const ModDropEntryHost* entries,
    int entryCount,
    bool wasKilledByPlayer,
    int playerBonusLevel);

struct ModCreativeItemHost
{
    int itemId;
    int aux;
    int categoryId;
    const char* displayName;
};

struct ModCreativeCategoryHost
{
    int categoryId;
    int baseGroup;
    const char* displayName;
};

extern int (*g_hostCollectModCreativeItems)(ModCreativeItemHost* outItems, int maxItems);
extern int (*g_hostCollectModCreativeCategories)(ModCreativeCategoryHost* outCategories, int maxCategories);