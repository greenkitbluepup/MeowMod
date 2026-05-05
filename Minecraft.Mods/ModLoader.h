#pragma once

#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ModAPI.h"

// ---------------------------------------------------------------------------
// ModConfig -- full engine-side definition of the opaque type in ModAPI.h.
// Mod DLLs see only the forward declaration and use it as a pointer.
// ---------------------------------------------------------------------------
struct ModConfig
{
    std::unordered_map<std::string, std::string> values;
};

struct ModCreativeItemHost;
struct ModCreativeCategoryHost;

struct ServerPacketHandlerEntry
{
    ModServerPacketHandlerFn fn = nullptr;
};

struct ClientPacketHandlerEntry
{
    ModClientPacketHandlerFn fn = nullptr;
};

// --- Registry entries -------------------------------------------------------
// These mirror ModBlockDef / ModItemDef / ModRecipeDef but with owned strings
// and the engine-allocated numeric ID.

struct BlockEntry
{
    std::string id;
    std::string textureName;
    int         material      = 0;   // MOD_MATERIAL_*
    float       hardness      = 1.0f;
    float       resistance    = 6.0f;
    int         lightEmission = 0;   // 0-15
    int         lightOpacity  = 15;  // 0-15
    int         shape         = 0;
    int         flags         = 0;   // MOD_BLOCK_*
    int         tileId        = 0;   // assigned after applyModTiles()
};

struct ItemEntry
{
    std::string id;
    std::string textureName;
    int         maxStack = 64;
    int         flags    = 0;  // MOD_ITEM_*
    int         itemId   = 0;  // assigned after applyModItems()
};

struct EntityEntry
{
    std::string id;
    int         networkTypeId = 0;
    int         primaryColor = 0;
    int         secondaryColor = 0;
    ModCreateEntityFn create = nullptr;
    ModTickEntityFn   tick = nullptr;
    ModHurtEntityFn   onHurt = nullptr;
    ModDeathEntityFn  onDeath = nullptr;
    ModEntitySaveFn   onSave = nullptr;
    ModEntityLoadFn   onLoad = nullptr;
};

struct ActiveEntityEntry
{
    int             runtimeId = 0;
    std::string     typeId;
    void*           handle = nullptr;
    void*           level = nullptr;
    float           x = 0.0f;
    float           y = 0.0f;
    float           z = 0.0f;
    ModTickEntityFn tick = nullptr;
    ModDeathEntityFn onDeath = nullptr;
    ModEntitySaveFn onSave = nullptr;
};

struct BlockRendererEntry
{
    int              blockId = 0;
    ModRenderBlockFn render = nullptr;
};

struct ItemRendererEntry
{
    int             itemId = 0;
    ModRenderItemFn render = nullptr;
};

struct EntityRendererEntry
{
    std::string      id;
    ModRenderEntityFn render = nullptr;
};

struct ProjectileRendererEntry
{
    std::string          id;
    ModRenderProjectileFn render = nullptr;
};

struct EventHandlerEntry
{
    std::string      eventName;
    ModEventHandlerFn fn = nullptr;
};

struct AssetEntry
{
    std::string type; // "texture" or "sound"
    std::string id;
    std::string path;
};

struct RecipeEntry
{
    int   outputId;
    int   outputCount;
    int   outputAux;
    int   recipeType;   // MOD_RECIPE_*
    int   width;
    int   height;
    int   grid[9];
    int   gridAux[9];
    float furnaceExp;
};

struct DropEntry
{
    int   itemId = 0;
    int   aux = 0;
    int   minCount = 1;
    int   maxCount = 1;
    float chance = 1.0f;
};

struct BlockDropEntry
{
    int blockId = 0;
    std::vector<DropEntry> drops;
};

struct EntityDropEntry
{
    std::string id;
    std::vector<DropEntry> drops;
};

struct LootTableEntry
{
    std::string id;
    std::vector<DropEntry> drops;
};

struct CreativeCategoryEntry
{
    int         id = 0;
    std::string name;
    std::string displayName;
    int         baseGroup = MOD_CREATIVE_GROUP_MISC;
};

struct CreativeItemEntry
{
    int         itemId = 0;
    int         aux = 0;
    int         categoryId = MOD_CREATIVE_GROUP_MISC;
    std::string displayName;
};

struct WorldGenEntry
{
    ModWorldGenFn generate; // fn pointer from mod
    int           weight;
};

struct BlockCallbackEntry
{
    int               blockId = 0;
    ModBlockCallbacks callbacks{};
};

struct TileEntityEntry
{
    std::string         id;
    int                 blockId = 0;
    ModCreateTileEntityFn create = nullptr;
    ModTileEntitySaveFn onSave = nullptr;
    ModTileEntityLoadFn onLoad = nullptr;
};

struct GuiEntry
{
    std::string id;
    int         blockId = 0;
    ModOpenGuiFn open = nullptr;
};

struct CommandEntry
{
    std::string name;
    std::string help;
    ModExecuteCommandFn execute = nullptr;
};

struct StructureEntry
{
    std::string       id;
    int               weight = 1;
    ModPlaceStructureFn place = nullptr;
};

struct StructurePaletteEntry
{
    int tileId = 0;
    int data   = 0;
};

struct StructureBlockEntry
{
    int x = 0;
    int y = 0;
    int z = 0;
    int paletteIndex = 0;
};

struct StructureTemplateEntry
{
    std::string id;
    std::vector<StructurePaletteEntry> palette;
    std::vector<StructureBlockEntry> blocks;
};

struct ProjectileEntry
{
    std::string        id;
    ModSpawnProjectileFn spawn = nullptr;
    ModUpdateProjectileFn update = nullptr;
};

struct ActiveProjectileEntry
{
    std::string id;
    void*       handle = nullptr;
    ModUpdateProjectileFn update = nullptr;
};

// ---------------------------------------------------------------------------

struct LoadedMod
{
    std::string   name;        // DLL filename
    void*         handle;
    ShutdownModFn shutdownFn;

    // Parsed mod.json fields (empty strings if no mod.json found)
    std::string   modId;
    std::string   displayName;
    std::string   version;
    std::string   description;
    std::string   author;

    struct DependencySpec
    {
        std::string id;
        std::string version;
    };

    std::vector<DependencySpec> requires;
    std::vector<DependencySpec> optional;
    std::vector<DependencySpec> conflicts;
    std::vector<std::string> permissions;

    // Parsed <modid>.cfg key/value pairs. nullptr if no config file found.
    ModConfig*    config = nullptr;
};

class ModLoader
{
public:
    void loadAllMods(const char* modsDir = "mods");
    void unloadAllMods();
    void printLoadReport() const;

    // Block / item / recipe / worldgen registration -----------------------
    int  registerBlock(const ModBlockDef* def);
    int  registerItem(const ModItemDef* def);
    void registerRecipe(const ModRecipeDef* def);
    void registerShapedRecipe(int outputId, int outputCount, int outputAux,
                              int width, int height,
                              const int* gridItems, const int* gridAux);
    void registerShapelessRecipe(int outputId, int outputCount, int outputAux,
                                 int count,
                                 const int* items, const int* auxVals);
    void registerSmeltingRecipe(int inputId, int inputAux,
                                int outputId, int outputCount, int outputAux,
                                float experience);
    void registerBlockDrop(int blockId,
                           const ModDropEntry* entries,
                           int entryCount);
    void registerEntityDrop(const char* entityId,
                            const ModDropEntry* entries,
                            int entryCount);
    void registerLootTable(const char* tableId,
                           const ModDropEntry* entries,
                           int entryCount);
    int  registerCreativeCategory(const char* id,
                                  const char* displayName,
                                  int baseGroup);
    void registerCreativeItem(const ModCreativeItem* def);
    bool loadModJson(const char* relativePath);
    bool loadModAssets(const char* manifestRelativePath);
    int  collectCreativeItems(ModCreativeItemHost* outItems, int maxItems) const;
    int  collectCreativeCategories(ModCreativeCategoryHost* outCategories, int maxCategories) const;
    bool applyBlockDrops(int blockId,
                         void* level,
                         int x, int y, int z,
                         int playerBonusLevel) const;
    bool applyEntityDrops(const wchar_t* entityId,
                          void* entity,
                          void* level,
                          bool wasKilledByPlayer,
                          int playerBonusLevel) const;
    void registerWorldGen(const ModWorldGenDef* def);
    void registerBlockCallbacks(int blockId, const ModBlockCallbacks* callbacks);
    void registerTileEntity(const ModTileEntityDef* def);
    void registerGui(const ModGuiDef* def);
    void registerCommand(const ModCommandDef* def);
    void registerStructure(const ModStructureDef* def);
    void registerStructureTemplate(const ModStructureTemplateDef* def);
    bool placeStructure(void* level,
                        const char* templateId,
                        int originX, int originY, int originZ,
                        int rotation);
    void registerServerPacketHandler(ModServerPacketHandlerFn fn);
    void registerClientPacketHandler(ModClientPacketHandlerFn fn);
    bool dispatchServerPacket(void* sender,
                              const char* channel,
                              const void* payload,
                              int size) const;
    bool dispatchClientPacket(const char* channel,
                              const void* payload,
                              int size) const;
    bool sendToServer(const char* channel, const void* payload, int size) const;
    bool sendToClient(void* player, const char* channel, const void* payload, int size) const;
    int  broadcastToTracking(void* level,
                             int x, int y, int z, int radius,
                             const char* channel,
                             const void* payload,
                             int size) const;
    std::string buildRegistryHandshakeBlob() const;
    bool validateRegistryHandshakeBlob(const char* remoteBlob, std::string* outReason) const;
    void registerProjectile(const ModProjectileDef* def);
    void* spawnProjectile(const char* id,
                          void* level,
                          void* owner,
                          float x, float y, float z,
                          float vx, float vy, float vz);
    void registerEntity(const ModEntityDef* def);
    void* spawnEntity(const char* id,
                      void* level,
                      float x, float y, float z);
    void tickActiveEntities();
    void registerBlockRenderer(const ModBlockRendererDef* def);
    void registerItemRenderer(const ModItemRendererDef* def);
    void registerEntityRenderer(const ModEntityRendererDef* def);
    void registerProjectileRenderer(const ModProjectileRendererDef* def);
    void registerEventHandler(const char* eventName, ModEventHandlerFn fn);
    void emitEvent(const ModEvent* e) const;
    void emitEvent(const char* eventName,
                   void* context,
                   const void* payload,
                   int payloadSize) const;
    bool dispatchBlockRenderer(void* level,
                               int x, int y, int z,
                               int blockId,
                               int blockData) const;
    bool dispatchItemRenderer(int itemId,
                              int aux,
                              int context,
                              void* itemEntity) const;
    bool dispatchEntityRenderer(void* entity,
                                const char* entityId,
                                float partialTick) const;
    bool dispatchProjectileRenderer(void* projectile,
                                    const char* projectileId,
                                    float partialTick) const;
    void renderActiveCustomEntities(float partialTick) const;
    void renderActiveProjectiles(float partialTick) const;
    void registerTexture(const char* id, const char* path);
    void registerSound(const char* id, const char* path);
    void registerLang(const char* id, const char* path);
    void registerModel(const char* id, const char* path);
    const std::vector<BlockEntry>&  blocks()    const { return m_blocks; }
    const std::vector<ItemEntry>&   items()     const { return m_items; }
    const std::vector<EntityEntry>& entities()  const { return m_entities; }
    const std::vector<AssetEntry>&  assets()    const { return m_assets; }
    const std::vector<RecipeEntry>& recipes()   const { return m_recipes; }

    // Called by ContentHooks (engine static-init lifecycle) ---------------
    void applyModTiles();    // create/register ModTile objects
    void applyModItems();    // create/register ModItem objects
    void applyModRecipes();  // push entries into Recipes / FurnaceRecipes
    void applyModTileEntities();
    void decorateChunk(void* levelHandle, int chunkX, int chunkZ, unsigned int seed);
    bool executeCommandText(void* sender, const wchar_t* commandLine) const;
    bool dispatchGuiOpenByBlock(int blockId,
                                void* player,
                                void* level,
                                int x, int y, int z) const;
    void dispatchTileEntitySave(const wchar_t* id, void* tileEntity, void* tag) const;
    void dispatchTileEntityLoad(const wchar_t* id, void* tileEntity, void* tag) const;
    void tickActiveProjectiles(void* level);

    bool dispatchBlockOnUse(int blockId,
                            void* level, int x, int y, int z,
                            void* player,
                            int clickedFace,
                            float clickX, float clickY, float clickZ) const;
    void dispatchBlockOnBreak(int blockId,
                              void* level, int x, int y, int z,
                              void* player) const;
    void dispatchBlockOnPlaced(int blockId,
                               void* level, int x, int y, int z,
                               void* player, int itemAux) const;
    void dispatchBlockOnNeighborChanged(int blockId,
                                        void* level, int x, int y, int z,
                                        int neighborId) const;
    void dispatchBlockOnTick(int blockId,
                             void* level, int x, int y, int z,
                             void* random) const;

    // Client tick ----------------------------------------------------------
    void registerClientTick(ClientTickFn fn);
    void tickClient();

    // Server tick ----------------------------------------------------------
    void registerServerTick(ServerTickFn fn);
    void tickServer();

    // Event bus ------------------------------------------------------------
    void registerLevelLoad(LevelLoadFn fn);
    void registerLevelUnload(LevelUnloadFn fn);
    void registerPlayerJoin(PlayerJoinFn fn);
    void registerPlayerLeave(PlayerLeaveFn fn);

    void dispatchLevelLoad(int isServer)   const;
    void dispatchLevelUnload(int isServer) const;
    void dispatchPlayerJoin(int entityId, const char* name)  const;
    void dispatchPlayerLeave(int entityId, const char* name) const;

    // Runtime dynamic light query ------------------------------------------
    void registerDynamicLightQuery(QueryDynamicLightFn fn);
    int  queryDynamicLight(int x, int y, int z) const;

    // Chunk bake snapshot API ----------------------------------------------
    void  registerPrepareChunkLightSnapshot(PrepareChunkLightSnapshotFn fn);
    void  registerQueryChunkSnapshotLight(QueryChunkSnapshotLightFn fn);
    void  registerDestroyChunkLightSnapshot(DestroyChunkLightSnapshotFn fn);

    void* prepareChunkLightSnapshot(int chunkX, int chunkY, int chunkZ) const;
    int   queryChunkSnapshotLight(void* snapshot, int x, int y, int z) const;
    void  destroyChunkLightSnapshot(void* snapshot) const;

    // Emitter feed ---------------------------------------------------------
    void registerBeginEmitterFeed(BeginEmitterFeedFn fn);
    void registerNotifyEmitter(NotifyEmitterFn fn);
    void registerEndEmitterFeed(EndEmitterFeedFn fn);

    // Tile changed ---------------------------------------------------------
    void registerTileChanged(TileChangedFn fn);
    void dispatchTileChanged(int x, int y, int z) const;

    // Called by the hook slot lambdas in loadAllMods.
    void dispatchBeginEmitterFeed() const  { if (m_beginEmitterFeed) m_beginEmitterFeed(); }
    void dispatchNotifyEmitter(int id, int x, int y, int z, int s) const { if (m_notifyEmitter) m_notifyEmitter(id, x, y, z, s); }
    void dispatchEndEmitterFeed() const    { if (m_endEmitterFeed) m_endEmitterFeed(); }

    ~ModLoader() { unloadAllMods(); }

private:
    std::vector<LoadedMod>             m_mods;
    std::vector<ClientTickFn>          m_clientTickCallbacks;
    std::vector<ServerTickFn>          m_serverTickCallbacks;
    std::vector<LevelLoadFn>           m_levelLoadCallbacks;
    std::vector<LevelUnloadFn>         m_levelUnloadCallbacks;
    std::vector<PlayerJoinFn>          m_playerJoinCallbacks;
    std::vector<PlayerLeaveFn>         m_playerLeaveCallbacks;
    std::vector<QueryDynamicLightFn>   m_dynamicLightQueries;
    PrepareChunkLightSnapshotFn        m_prepareChunkLightSnapshot  = nullptr;
    QueryChunkSnapshotLightFn          m_queryChunkSnapshotLight    = nullptr;
    DestroyChunkLightSnapshotFn        m_destroyChunkLightSnapshot  = nullptr;
    BeginEmitterFeedFn                 m_beginEmitterFeed           = nullptr;
    NotifyEmitterFn                    m_notifyEmitter              = nullptr;
    EndEmitterFeedFn                   m_endEmitterFeed             = nullptr;
    std::vector<TileChangedFn>         m_tileChangedCallbacks;
    std::vector<BlockEntry>            m_blocks;
    std::vector<ItemEntry>             m_items;
    std::vector<EntityEntry>           m_entities;
    std::vector<AssetEntry>            m_assets;
    std::vector<RecipeEntry>           m_recipes;
    std::vector<BlockDropEntry>        m_blockDrops;
    std::vector<EntityDropEntry>       m_entityDrops;
    std::vector<LootTableEntry>        m_lootTables;
    std::vector<CreativeCategoryEntry> m_creativeCategories;
    std::vector<CreativeItemEntry>     m_creativeItems;
    std::vector<WorldGenEntry>         m_worldGens;
    std::vector<BlockCallbackEntry>    m_blockCallbacks;
    std::vector<TileEntityEntry>       m_tileEntities;
    std::vector<GuiEntry>              m_guis;
    std::vector<CommandEntry>          m_commands;
    std::vector<StructureEntry>        m_structures;
    std::vector<StructureTemplateEntry> m_structureTemplates;
    std::vector<ServerPacketHandlerEntry> m_serverPacketHandlers;
    std::vector<ClientPacketHandlerEntry> m_clientPacketHandlers;
    std::vector<ProjectileEntry>       m_projectiles;
    std::vector<ActiveProjectileEntry> m_activeProjectiles;
    std::vector<ActiveEntityEntry>     m_activeEntities;
    std::vector<BlockRendererEntry>    m_blockRenderers;
    std::vector<ItemRendererEntry>     m_itemRenderers;
    std::vector<EntityRendererEntry>   m_entityRenderers;
    std::vector<ProjectileRendererEntry> m_projectileRenderers;
    std::vector<EventHandlerEntry>     m_eventHandlers;

    // Next available IDs for mod-added content.
    // Tile IDs 256-4095 (vanilla max is ~173).
    // Item IDs 1000-31999 final game IDs (constructor param = finalId - 256).
    int m_nextTileId = 256;
    int m_nextItemId = 1000; // final game ID
    int m_nextCreativeCategoryId = 1000;
    int m_nextRuntimeEntityId = 1;

    std::string m_currentModDir;
    std::vector<std::string> m_loadedJsonPathsForCurrentMod;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_permissionsByMod;
    std::string m_activePermissionModId;
    bool m_permissionsStrict = false;

    bool requirePermission(const char* permission, const char* apiName) const;

    bool loadModJsonFile(const std::string& absolutePath);
    bool loadModAssetsFile(const std::string& absolutePath);
    void autoScanModContentJson(const std::string& modDir);

    static void hostLog(const char* message);
};

extern ModLoader g_modLoader;
