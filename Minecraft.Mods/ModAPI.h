#pragma once

// ModAPI.h - Shared ABI between the engine and mod DLLs.
// Keep this header self-contained so mod authors can include it without
// pulling in any engine headers.

#define MOD_API_VERSION 25

// Opaque handle to this mod's configuration. Do not dereference directly.
// Pass the pointer to getConfig only. Valid for the lifetime of the mod.
struct ModConfig;

// ---------------------------------------------------------------------------
// --- Block / item / recipe / worldgen definition structs ---
//
// Pass by pointer to the register* functions during InitMod.
// The engine copies all fields; you do not need to keep the struct alive.
// ---------------------------------------------------------------------------

// --- Material constants (ModBlockDef::material) ---
#define MOD_MATERIAL_ROCK    0
#define MOD_MATERIAL_DIRT    1
#define MOD_MATERIAL_WOOD    2
#define MOD_MATERIAL_METAL   3
#define MOD_MATERIAL_CLOTH   4
#define MOD_MATERIAL_SAND    5
#define MOD_MATERIAL_GLASS   6
#define MOD_MATERIAL_PLANT   7

// --- Block flags (ModBlockDef::flags, bitmask) ---
#define MOD_BLOCK_SOLID      (1 << 0)
#define MOD_BLOCK_OPAQUE     (1 << 1)
#define MOD_BLOCK_TICKING    (1 << 2)
#define MOD_BLOCK_ENTITY     (1 << 3)

// --- Item flags (ModItemDef::flags, bitmask) ---
#define MOD_ITEM_NO_REPAIR   (1 << 0)
#define MOD_ITEM_GLOW        (1 << 1)

// --- Recipe types (ModRecipeDef::recipeType) ---
#define MOD_RECIPE_SHAPED    0
#define MOD_RECIPE_SHAPELESS 1
#define MOD_RECIPE_FURNACE   2

struct ModBlockDef
{
    const char* id;
    const char* textureName;
    int         material;
    float       hardness;
    float       resistance;
    int         lightEmission;
    int         lightOpacity;
    int         shape;
    int         flags;
};

struct ModItemDef
{
    const char* id;
    const char* textureName;
    int         maxStack;
    int         flags;
};

#define MOD_RECIPE_GRID_MAX 9

struct ModRecipeDef
{
    int   outputId;
    int   outputCount;
    int   outputAux;
    int   recipeType;
    int   width;
    int   height;
    int   grid[MOD_RECIPE_GRID_MAX];
    int   gridAux[MOD_RECIPE_GRID_MAX];
    float furnaceExp;
};

struct ModDropEntry
{
    int   itemId;
    int   aux;
    int   minCount;
    int   maxCount;
    float chance;
};

// Creative inventory groups.
#define MOD_CREATIVE_GROUP_BUILDING_BLOCKS       0
#define MOD_CREATIVE_GROUP_DECORATION            1
#define MOD_CREATIVE_GROUP_REDSTONE              2
#define MOD_CREATIVE_GROUP_TRANSPORT             3
#define MOD_CREATIVE_GROUP_MATERIALS             4
#define MOD_CREATIVE_GROUP_FOOD                  5
#define MOD_CREATIVE_GROUP_TOOLS_ARMOUR_WEAPONS  6
#define MOD_CREATIVE_GROUP_BREWING               7
#define MOD_CREATIVE_GROUP_MISC                  11

struct ModCreativeItem
{
    int         itemId;
    int         aux;
    int         categoryId;
    const char* displayName;
};

// --- Generic item stack ABI for player inventory / hand access ---
// Empty stack convention:
//   itemId = 0
//   aux    = 0
//   count  = 0
struct ModItemStack
{
    int itemId;
    int aux;
    int count;
};

// --- World generation definition ---
typedef void (*ModPlaceBlockFn)(void* lvl, int x, int y, int z, int tileId, int data);
typedef void (*ModWorldGenFn)(void* lvl, int chunkX, int chunkZ,
    unsigned int seed, ModPlaceBlockFn placeBlock);

struct ModWorldGenDef
{
    ModWorldGenFn generate;
    int           weight;
};

// --- Block interaction callbacks ---
using ModBlockOnUseFn = bool (*)(void* level, int x, int y, int z,
    void* player,
    int clickedFace,
    float clickX, float clickY, float clickZ);

using ModBlockOnBreakFn = void (*)(void* level, int x, int y, int z, void* player);
using ModBlockOnPlacedFn = void (*)(void* level, int x, int y, int z, void* player, int itemAux);
using ModBlockOnNeighborChangedFn = void (*)(void* level, int x, int y, int z, int neighborId);
using ModBlockOnTickFn = void (*)(void* level, int x, int y, int z, void* random);

struct ModBlockCallbacks
{
    ModBlockOnUseFn             onUse;
    ModBlockOnBreakFn           onBreak;
    ModBlockOnPlacedFn          onPlaced;
    ModBlockOnNeighborChangedFn onNeighborChanged;
    ModBlockOnTickFn            onTick;
};

// --- Tile entities / GUI / commands ---
using ModCreateTileEntityFn = void* (*)(void);
using ModTileEntitySaveFn = void (*)(void* tileEntity, void* tag);
using ModTileEntityLoadFn = void (*)(void* tileEntity, void* tag);

struct ModTileEntityDef
{
    const char* id;
    int                   blockId;
    ModCreateTileEntityFn create;
    ModTileEntitySaveFn   onSave;
    ModTileEntityLoadFn   onLoad;
};

using ModOpenGuiFn = void (*)(void* player, void* level, int x, int y, int z);

struct ModGuiDef
{
    const char* id;
    int          blockId;
    ModOpenGuiFn open;
};

using ModExecuteCommandFn = bool (*)(void* sender, const char* argsLine);

struct ModCommandDef
{
    const char* name;
    const char* help;
    ModExecuteCommandFn  execute;
};

// --- Structures ---
using ModPlaceStructureFn = bool (*)(void* level, int chunkX, int chunkZ,
    unsigned int seed, ModPlaceBlockFn placeBlock);

struct ModStructureDef
{
    const char* id;
    int                 weight;
    ModPlaceStructureFn place;
};

#define MOD_STRUCTURE_ROT_0   0
#define MOD_STRUCTURE_ROT_90  1
#define MOD_STRUCTURE_ROT_180 2
#define MOD_STRUCTURE_ROT_270 3

struct ModStructurePaletteEntry
{
    int tileId;
    int data;
};

struct ModStructureBlockEntry
{
    int x;
    int y;
    int z;
    int paletteIndex;
};

struct ModStructureTemplateDef
{
    const char* id;
    int paletteCount;
    const ModStructurePaletteEntry* palette;
    int blockCount;
    const ModStructureBlockEntry* blocks;
};

// --- Projectiles ---
using ModSpawnProjectileFn = void* (*)(void* level, void* owner,
    float x, float y, float z,
    float vx, float vy, float vz);

using ModUpdateProjectileFn = bool (*)(void* projectile, void* level);

struct ModProjectileDef
{
    const char* id;
    ModSpawnProjectileFn  spawn;
    ModUpdateProjectileFn update;
};

// --- Custom entities ---
using ModCreateEntityFn = void* (*)(void* level, float x, float y, float z);
using ModTickEntityFn = bool (*)(void* entity, void* level, float* x, float* y, float* z);
using ModHurtEntityFn = void (*)(void* entity, void* source, float amount);
using ModDeathEntityFn = void (*)(void* entity, void* source);
using ModEntitySaveFn = int (*)(void* entity, void* outBuffer, int outBufferSize);
using ModEntityLoadFn = void (*)(void* entity, const void* data, int size);

struct ModEntityDef
{
    const char* id;
    int               networkTypeId;
    int               eggPrimaryColor;
    int               eggSecondaryColor;
    ModCreateEntityFn create;
    ModTickEntityFn   tick;
    ModHurtEntityFn   onHurt;
    ModDeathEntityFn  onDeath;
    ModEntitySaveFn   onSave;
    ModEntityLoadFn   onLoad;
};

// --- Client rendering hooks ---
using ModRenderBlockFn = bool (*)(void* level,
    int x, int y, int z,
    int blockId,
    int blockData);

struct ModBlockRendererDef
{
    int              blockId;
    ModRenderBlockFn render;
};

using ModRenderItemFn = bool (*)(int itemId,
    int aux,
    int context,
    void* itemEntity);

struct ModItemRendererDef
{
    int             itemId;
    ModRenderItemFn render;
};

using ModRenderEntityFn = bool (*)(void* entity,
    const char* entityId,
    float partialTick);

struct ModEntityRendererDef
{
    const char* id;
    ModRenderEntityFn render;
};

using ModRenderProjectileFn = bool (*)(void* projectile,
    const char* projectileId,
    float partialTick);

struct ModProjectileRendererDef
{
    const char* id;
    ModRenderProjectileFn render;
};

// --- Event bus ---
struct ModEvent
{
    const char* name;
    void* context;
    const void* payload;
    int         payloadSize;
};

using ModEventHandlerFn = void (*)(const ModEvent* e);

// --- Metadata ---
struct ModMetadata
{
    const char* id;
    const char* name;
    const char* version;
    const char* description;
    const char* author;
};

// --- Registration function typedefs ---
using RegisterBlockFn = int (*)(const ModBlockDef* def);
using RegisterItemFn = int (*)(const ModItemDef* def);
using RegisterRecipeFn = void (*)(const ModRecipeDef* def);

using RegisterShapedRecipeFn = void (*)(int outputId, int outputCount, int outputAux,
    int width, int height,
    const int* gridItems, const int* gridAux);

using RegisterShapelessRecipeFn = void (*)(int outputId, int outputCount, int outputAux,
    int count,
    const int* items, const int* auxVals);

using RegisterSmeltingRecipeFn = void (*)(int inputId, int inputAux,
    int outputId, int outputCount, int outputAux,
    float experience);

using RegisterBlockDropFn = void (*)(int blockId,
    const ModDropEntry* entries,
    int entryCount);

using RegisterEntityDropFn = void (*)(const char* entityId,
    const ModDropEntry* entries,
    int entryCount);

using RegisterLootTableFn = void (*)(const char* tableId,
    const ModDropEntry* entries,
    int entryCount);

using RegisterCreativeItemFn = void (*)(const ModCreativeItem* def);

using RegisterCreativeCategoryFn = int (*)(const char* id,
    const char* displayName,
    int baseGroup);

using RegisterEventHandlerFn = void (*)(const char* eventName, ModEventHandlerFn fn);
using EmitEventFn = void (*)(const ModEvent* e);

using LoadModJsonFn = bool (*)(const char* relativePath);

using RegisterTextureFn = void (*)(const char* id, const char* path);
using RegisterSoundFn = void (*)(const char* id, const char* path);
using RegisterLangFn = void (*)(const char* id, const char* path);
using RegisterModelFn = void (*)(const char* id, const char* path);
using LoadModAssetsFn = bool (*)(const char* manifestRelativePath);

using RegisterWorldGenFn = void (*)(const ModWorldGenDef* def);
using RegisterBlockCallbacksFn = void (*)(int blockId, const ModBlockCallbacks* callbacks);
using RegisterTileEntityFn = void (*)(const ModTileEntityDef* def);
using RegisterGuiFn = void (*)(const ModGuiDef* def);
using RegisterCommandFn = void (*)(const ModCommandDef* def);
using RegisterStructureFn = void (*)(const ModStructureDef* def);
using RegisterStructureTemplateFn = void (*)(const ModStructureTemplateDef* def);
using RegisterProjectileFn = void (*)(const ModProjectileDef* def);

using ModServerPacketHandlerFn = bool (*)(void* sender,
    const char* channel,
    const void* payload,
    int size);

using ModClientPacketHandlerFn = bool (*)(const char* channel,
    const void* payload,
    int size);

using PlaceStructureFn = bool (*)(void* level,
    const char* templateId,
    int originX, int originY, int originZ,
    int rotation);

using PlaceStructureFromSenderFn = bool (*)(void* sender,
    const char* templateId,
    int offsetX, int offsetY, int offsetZ,
    int rotation);

using GetConfigFn = const char* (*)(ModConfig* cfg, const char* key, const char* defaultVal);

// --- Player hand / offhand access ---
// All functions are game-thread only.
// The player pointer is opaque. Use getLocalPlayer() on the client.
using GetLocalPlayerFn = void* (*)(void);

using GetPlayerSelectedItemFn = bool (*)(void* player, ModItemStack* out);
using SetPlayerSelectedItemFn = bool (*)(void* player, const ModItemStack* stack);

using GetPlayerOffhandItemFn = bool (*)(void* player, ModItemStack* out);
using SetPlayerOffhandItemFn = bool (*)(void* player, const ModItemStack* stack);

using SwapPlayerHandsFn = bool (*)(void* player);

// --- Tick / lifecycle callbacks ---
using ServerTickFn = void (*)(void);
using LevelLoadFn = void (*)(int isServer);
using LevelUnloadFn = void (*)(int isServer);
using PlayerJoinFn = void (*)(int entityId, const char* name);
using PlayerLeaveFn = void (*)(int entityId, const char* name);
using ClientTickFn = void (*)(void);

// --- Dynamic light runtime query ---
using QueryDynamicLightFn = int (*)(int x, int y, int z);

// --- Chunk bake snapshot API ---
using PrepareChunkLightSnapshotFn = void* (*)(int chunkX, int chunkY, int chunkZ);
using QueryChunkSnapshotLightFn = int (*)(void* snapshot, int x, int y, int z);
using DestroyChunkLightSnapshotFn = void (*)(void* snapshot);

// --- Emitter feed API ---
using BeginEmitterFeedFn = void (*)(void);
using NotifyEmitterFn = void (*)(int entityId, int x, int y, int z, int strength);
using EndEmitterFeedFn = void (*)(void);

// Called whenever any tile/block in the client world changes.
using TileChangedFn = void (*)(int x, int y, int z);

// ---------------------------------------------------------------------------
// ModHostAPI
// ---------------------------------------------------------------------------

struct ModHostAPI
{
    // Check this first in InitMod. Abort if != MOD_API_VERSION.
    int apiVersion;

    // Metadata parsed from mod.json beside the DLL, or nullptr if unavailable.
    const ModMetadata* metadata;

    // This mod's configuration, parsed from <modid>.cfg beside the DLL.
    ModConfig* config;

    // Look up a key in this mod's config file.
    GetConfigFn getConfig;

    // Write a message to the engine debug output.
    void (*log)(const char* message);

    // --- Register callbacks ---

    void (*registerClientTick)(ClientTickFn fn);

    void (*registerDynamicLightQuery)(QueryDynamicLightFn fn);

    void (*registerPrepareChunkLightSnapshot)(PrepareChunkLightSnapshotFn fn);
    void (*registerQueryChunkSnapshotLight)(QueryChunkSnapshotLightFn fn);
    void (*registerDestroyChunkLightSnapshot)(DestroyChunkLightSnapshotFn fn);

    void (*registerBeginEmitterFeed)(BeginEmitterFeedFn fn);
    void (*registerNotifyEmitter)(NotifyEmitterFn fn);
    void (*registerEndEmitterFeed)(EndEmitterFeedFn fn);

    void (*registerTileChanged)(TileChangedFn fn);

    // --- Block / item registration ---

    int (*registerBlock)(const ModBlockDef* def);
    int (*registerItem)(const ModItemDef* def);

    void (*registerEntity)(const ModEntityDef* def);

    void* (*spawnEntity)(const char* id,
        void* level,
        float x, float y, float z);

    // --- Asset registration ---

    void (*registerTexture)(const char* id, const char* path);
    void (*registerSound)(const char* id, const char* path);
    void (*registerLang)(const char* id, const char* path);
    void (*registerModel)(const char* id, const char* path);

    // --- Recipes ---

    void (*registerRecipe)(const ModRecipeDef* def);

    void (*registerShapedRecipe)(int outputId, int outputCount, int outputAux,
        int width, int height,
        const int* gridItems, const int* gridAux);

    void (*registerShapelessRecipe)(int outputId, int outputCount, int outputAux,
        int count,
        const int* items, const int* auxVals);

    void (*registerSmeltingRecipe)(int inputId, int inputAux,
        int outputId, int outputCount, int outputAux,
        float experience);

    // --- Loot / drops ---

    void (*registerBlockDrop)(int blockId,
        const ModDropEntry* entries,
        int entryCount);

    void (*registerEntityDrop)(const char* entityId,
        const ModDropEntry* entries,
        int entryCount);

    void (*registerLootTable)(const char* tableId,
        const ModDropEntry* entries,
        int entryCount);

    // --- Creative inventory ---

    int (*registerCreativeCategory)(const char* id,
        const char* displayName,
        int baseGroup);

    void (*registerCreativeItem)(const ModCreativeItem* def);

    // --- Data / assets ---

    bool (*loadModJson)(const char* relativePath);
    bool (*loadModAssets)(const char* manifestRelativePath);

    // --- Client rendering hooks ---

    void (*registerBlockRenderer)(const ModBlockRendererDef* def);
    void (*registerItemRenderer)(const ModItemRendererDef* def);
    void (*registerEntityRenderer)(const ModEntityRendererDef* def);
    void (*registerProjectileRenderer)(const ModProjectileRendererDef* def);

    // --- Event bus ---

    void (*registerEventHandler)(const char* eventName, ModEventHandlerFn fn);
    void (*emitEvent)(const ModEvent* e);

    // --- World/content registration ---

    void (*registerWorldGen)(const ModWorldGenDef* def);
    void (*registerBlockCallbacks)(int blockId, const ModBlockCallbacks* callbacks);

    void (*registerTileEntity)(const ModTileEntityDef* def);
    void (*registerGui)(const ModGuiDef* def);
    void (*registerCommand)(const ModCommandDef* def);
    void (*registerStructure)(const ModStructureDef* def);
    void (*registerStructureTemplate)(const ModStructureTemplateDef* def);
    void (*registerProjectile)(const ModProjectileDef* def);

    // --- Networking ---

    void (*registerServerPacketHandler)(ModServerPacketHandlerFn fn);
    void (*registerClientPacketHandler)(ModClientPacketHandlerFn fn);

    bool (*sendToServer)(const char* channel, const void* payload, int size);

    bool (*sendToClient)(void* player,
        const char* channel,
        const void* payload,
        int size);

    int (*broadcastToTracking)(void* level,
        int x, int y, int z,
        int radius,
        const char* channel,
        const void* payload,
        int size);

    // --- Structures ---

    bool (*placeStructure)(void* level,
        const char* templateId,
        int originX, int originY, int originZ,
        int rotation);

    bool (*placeStructureFromSender)(void* sender,
        const char* templateId,
        int offsetX, int offsetY, int offsetZ,
        int rotation);

    // --- Tile entity sync ---

    bool (*markModTileEntityDirty)(void* tileEntity);

    // --- Projectile spawning ---

    void* (*spawnProjectile)(const char* id,
        void* level,
        void* owner,
        float x, float y, float z,
        float vx, float vy, float vz);

    // --- Server/client lifecycle ---

    void (*registerServerTick)(ServerTickFn fn);

    void (*registerLevelLoad)(LevelLoadFn fn);
    void (*registerLevelUnload)(LevelUnloadFn fn);

    void (*registerPlayerJoin)(PlayerJoinFn fn);
    void (*registerPlayerLeave)(PlayerLeaveFn fn);

    // --- Host-provided world queries ---

    int (*getTileOpacity)(int x, int y, int z);

    void (*markRegionDirty)(int x0, int y0, int z0,
        int x1, int y1, int z1);

    // --- Player hand / offhand access ---
    //
    // Game-thread only.
    //
    // Empty stack convention:
    //   itemId = 0
    //   aux    = 0
    //   count  = 0
    //
    // These intentionally expose only selected hand and offhand, not the full
    // inventory array.

    void* (*getLocalPlayer)(void);

    bool (*getPlayerSelectedItem)(void* player, ModItemStack* out);
    bool (*setPlayerSelectedItem)(void* player, const ModItemStack* stack);

    bool (*getPlayerOffhandItem)(void* player, ModItemStack* out);
    bool (*setPlayerOffhandItem)(void* player, const ModItemStack* stack);

    bool (*swapPlayerHands)(void* player);
};

// Required exports -- every mod DLL must define both with C linkage.
using InitModFn = bool (*)(ModHostAPI* api);
using ShutdownModFn = void (*)(void);