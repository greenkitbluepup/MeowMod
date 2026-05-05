#include "ModAPI.h"
#include <cstring>

struct DummyEntity
{
    float x;
    float y;
    float z;
    int   ticks;
};

#include "ModAPI.h"

// ---------------------------------------------------------------------------
// TemplateMod  --  copy this folder to start a new mod.
// ---------------------------------------------------------------------------

static ModHostAPI g_api{};
static int g_glowBlockId   = 0;
static int g_crystalItemId = 0;
static int g_projectileTicks = 0;
static int g_creativeCategoryId = MOD_CREATIVE_GROUP_MISC;
static const char* kTemplateStructureId = "templatemod:sample_structure";

static const ModStructurePaletteEntry kTemplatePalette[] = {
    { 0, 0 },
};

static const ModStructureBlockEntry kTemplateBlocks[] = {
    { 0, 0, 0, 0 },
    { 1, 0, 0, 0 },
    { -1, 0, 0, 0 },
    { 0, 0, 1, 0 },
    { 0, 0, -1, 0 },
    { 0, 1, 0, 0 },
};

static void OnDecorateChunk(void* lvl, int chunkX, int chunkZ,
                             unsigned int seed, ModPlaceBlockFn placeBlock)
{
    (void)placeBlock;
    if (!g_glowBlockId || !g_api.placeStructure) return;
    int bx = (chunkX << 4) + 8;
    int bz = (chunkZ << 4) + 8;
    int rot = static_cast<int>(seed & 3u);
    g_api.placeStructure(lvl, kTemplateStructureId, bx, 70, bz, rot);
}

static void OnClientTick() {}
static void OnServerTick() {}
static void OnLevelLoad(int isServer)
{
    g_api.log(isServer ? "[TemplateMod] server level loaded" : "[TemplateMod] client level loaded");
}
static void OnLevelUnload(int isServer) { (void)isServer; }
static void OnPlayerJoin(int entityId, const char* name) { (void)entityId; (void)name; }
static void OnPlayerLeave(int entityId, const char* name) { (void)entityId; (void)name; }

static bool OnGlowBlockUse(void* level, int x, int y, int z,
                           void* player,
                           int clickedFace,
                           float clickX, float clickY, float clickZ)
{
    (void)level; (void)x; (void)y; (void)z;
    (void)player; (void)clickedFace;
    (void)clickX; (void)clickY; (void)clickZ;
    g_api.log("[TemplateMod] glow block used");
    if (g_api.placeStructure)
        g_api.placeStructure(level, kTemplateStructureId, x, y + 1, z, MOD_STRUCTURE_ROT_0);
    return false; // false => let normal block logic continue
}

static void OnGlowBlockBreak(void* level, int x, int y, int z, void* player)
{
    (void)level; (void)x; (void)y; (void)z; (void)player;
    g_api.log("[TemplateMod] glow block broken");
}

static bool OnTemplateCommand(void* sender, const char* argsLine)
{
    (void)argsLine;
    g_api.log("[TemplateMod] command executed");
    if (g_api.placeStructureFromSender)
        g_api.placeStructureFromSender(sender, kTemplateStructureId, 0, 1, 0, MOD_STRUCTURE_ROT_90);
    if (g_api.spawnProjectile)
    {
        g_api.spawnProjectile("templatemod:dummy_projectile",
                              nullptr,
                              sender,
                              0.0f, 70.0f, 0.0f,
                              0.0f, 0.1f, 0.0f);
    }
    if (g_api.spawnEntity)
        g_api.spawnEntity("templatemod:dummy_entity", nullptr, 0.0f, 72.0f, 0.0f);
    return true;
}

static void* OnCreateDummyEntity(void* level, float x, float y, float z)
{
    (void)level;
    DummyEntity* e = new DummyEntity();
    e->x = x;
    e->y = y;
    e->z = z;
    e->ticks = 0;
    return e;
}

static bool OnTickDummyEntity(void* entity, void* level, float* x, float* y, float* z)
{
    (void)level;
    DummyEntity* e = static_cast<DummyEntity*>(entity);
    if (!e || !x || !y || !z)
        return false;

    ++e->ticks;
    e->y += 0.02f;
    *x = e->x;
    *y = e->y;
    *z = e->z;
    return e->ticks < 200;
}

static void OnDeathDummyEntity(void* entity, void* source)
{
    (void)source;
    DummyEntity* e = static_cast<DummyEntity*>(entity);
    delete e;
}

static int OnSaveDummyEntity(void* entity, void* outBuffer, int outBufferSize)
{
    DummyEntity* e = static_cast<DummyEntity*>(entity);
    if (!e)
        return 0;
    if (!outBuffer)
        return static_cast<int>(sizeof(DummyEntity));
    if (outBufferSize < static_cast<int>(sizeof(DummyEntity)))
        return 0;
    std::memcpy(outBuffer, e, sizeof(DummyEntity));
    return static_cast<int>(sizeof(DummyEntity));
}

static void OnLoadDummyEntity(void* entity, const void* data, int size)
{
    DummyEntity* e = static_cast<DummyEntity*>(entity);
    if (!e || !data || size < static_cast<int>(sizeof(DummyEntity)))
        return;
    std::memcpy(e, data, sizeof(DummyEntity));
}

static bool OnServerPacket(void* sender, const char* channel, const void* payload, int size)
{
    (void)payload;
    (void)size;
    if (!channel) return false;

    if (strcmp(channel, "templatemod:gui_click") == 0)
    {
        const char* ack = "ok";
        if (g_api.sendToClient)
            g_api.sendToClient(sender, "templatemod:gui_state", ack, 2);
        return true;
    }
    return false;
}

static void OnAnyEvent(const ModEvent* e)
{
    if (!e || !e->name) return;
    char msg[256];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE, "[TemplateMod] event: %s", e->name);
    g_api.log(msg);
}

static bool OnClientPacket(const char* channel, const void* payload, int size)
{
    (void)payload;
    (void)size;
    if (!channel) return false;
    if (strcmp(channel, "templatemod:gui_state") == 0)
    {
        g_api.log("[TemplateMod] gui sync update");
        return true;
    }
    return false;
}

static void* OnSpawnDummyProjectile(void* level, void* owner,
                                    float x, float y, float z,
                                    float vx, float vy, float vz)
{
    (void)level; (void)owner;
    (void)x; (void)y; (void)z;
    (void)vx; (void)vy; (void)vz;
    g_projectileTicks = 0;
    return reinterpret_cast<void*>(0x1); // demo handle
}

static bool OnUpdateDummyProjectile(void* projectile, void* level)
{
    (void)projectile; (void)level;
    ++g_projectileTicks;
    return g_projectileTicks < 60; // survive 60 ticks
}

extern "C" __declspec(dllexport) bool InitMod(ModHostAPI* api)
{
    if (!api || api->apiVersion != MOD_API_VERSION)
        return false;

    g_api = *api;
    g_api.log(api->metadata ? api->metadata->name : "[TemplateMod] loaded (no mod.json)");

    if (api->loadModJson)
    {
        api->loadModJson("content/blocks.json");
        api->loadModJson("content/items.json");
        api->loadModJson("content/recipes.json");
        api->loadModJson("content/loot_tables.json");
    }

    if (api->loadModAssets)
        api->loadModAssets("assets/manifest.json");

    ModBlockDef bd{};
    bd.id            = "templatemod:glow_block";
    bd.textureName   = "templatemod:glow_block";
    bd.material      = MOD_MATERIAL_ROCK;
    bd.hardness      = 1.5f;
    bd.resistance    = 6.0f;
    bd.lightEmission = 12;
    bd.lightOpacity  = 15;
    bd.shape         = 0;
    bd.flags         = MOD_BLOCK_SOLID | MOD_BLOCK_OPAQUE;
    g_glowBlockId    = api->registerBlock(&bd);

    ModItemDef idef{};
    idef.id          = "templatemod:crystal";
    idef.textureName = "templatemod:crystal";
    idef.maxStack    = 16;
    idef.flags       = MOD_ITEM_GLOW;
    g_crystalItemId  = api->registerItem(&idef);

    if (api->registerCreativeCategory)
        g_creativeCategoryId = api->registerCreativeCategory("templatemod:main", "Template Mod", MOD_CREATIVE_GROUP_MISC);

    if (api->registerCreativeItem)
    {
        if (g_glowBlockId)
        {
            ModCreativeItem ci{};
            ci.itemId = g_glowBlockId;
            ci.aux = 0;
            ci.categoryId = g_creativeCategoryId;
            ci.displayName = "Glow Block";
            api->registerCreativeItem(&ci);
        }

        if (g_crystalItemId)
        {
            ModCreativeItem ci{};
            ci.itemId = g_crystalItemId;
            ci.aux = 0;
            ci.categoryId = g_creativeCategoryId;
            ci.displayName = "Crystal";
            api->registerCreativeItem(&ci);
        }
    }

    if (g_glowBlockId && g_crystalItemId)
    {
        int gridItems[4] = { g_crystalItemId, g_crystalItemId, g_crystalItemId, g_crystalItemId };
        int gridAux[4]   = { 0, 0, 0, 0 };
        api->registerShapedRecipe(g_glowBlockId, 1, 0, 2, 2, gridItems, gridAux);
    }

    if (g_crystalItemId)
    {
        api->registerSmeltingRecipe(g_crystalItemId, 0, g_crystalItemId, 1, 0, 0.5f);
    }

    if (g_glowBlockId)
    {
        ModDropEntry blockDrops[1] = {
            { g_crystalItemId, 0, 1, 2, 1.0f }
        };
        api->registerBlockDrop(g_glowBlockId, blockDrops, 1);

        ModDropEntry pigDrops[1] = {
            { g_crystalItemId, 0, 1, 1, 0.25f }
        };
        api->registerEntityDrop("Pig", pigDrops, 1);

        ModDropEntry sampleLoot[2] = {
            { g_crystalItemId, 0, 1, 3, 1.0f },
            { g_glowBlockId,   0, 1, 1, 0.2f }
        };
        api->registerLootTable("templatemod:sample_structure_chest", sampleLoot, 2);

        ModStructurePaletteEntry palette[sizeof(kTemplatePalette) / sizeof(kTemplatePalette[0])];
        for (int i = 0; i < static_cast<int>(sizeof(palette) / sizeof(palette[0])); ++i)
            palette[i] = kTemplatePalette[i];
        palette[0].tileId = g_glowBlockId;

        ModStructureTemplateDef templ{};
        templ.id = kTemplateStructureId;
        templ.paletteCount = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
        templ.palette = palette;
        templ.blockCount = static_cast<int>(sizeof(kTemplateBlocks) / sizeof(kTemplateBlocks[0]));
        templ.blocks = kTemplateBlocks;
        api->registerStructureTemplate(&templ);

        ModWorldGenDef wg{};
        wg.generate = &OnDecorateChunk;
        wg.weight   = 1;
        api->registerWorldGen(&wg);

        ModBlockCallbacks cbs{};
        cbs.onUse = &OnGlowBlockUse;
        cbs.onBreak = &OnGlowBlockBreak;
        api->registerBlockCallbacks(g_glowBlockId, &cbs);

        ModTileEntityDef te{};
        te.id = "templatemod:dummy_te";
        te.blockId = g_glowBlockId;
        te.create = nullptr;
        api->registerTileEntity(&te);

        ModGuiDef gui{};
        gui.id = "templatemod:dummy_gui";
        gui.blockId = g_glowBlockId;
        gui.open = nullptr;
        api->registerGui(&gui);

    }

    ModCommandDef cmd{};
    cmd.name = "templatemod";
    cmd.help = "Template command";
    cmd.execute = &OnTemplateCommand;
    api->registerCommand(&cmd);

    ModProjectileDef pr{};
    pr.id = "templatemod:dummy_projectile";
    pr.spawn = &OnSpawnDummyProjectile;
    pr.update = &OnUpdateDummyProjectile;
    api->registerProjectile(&pr);

    ModEntityDef ent{};
    ent.id = "templatemod:dummy_entity";
    ent.networkTypeId = 9001;
    ent.eggPrimaryColor = 0x44AAFF;
    ent.eggSecondaryColor = 0x113355;
    ent.create = &OnCreateDummyEntity;
    ent.tick = &OnTickDummyEntity;
    ent.onHurt = nullptr;
    ent.onDeath = &OnDeathDummyEntity;
    ent.onSave = &OnSaveDummyEntity;
    ent.onLoad = &OnLoadDummyEntity;
    api->registerEntity(&ent);

    g_api.registerClientTick(&OnClientTick);
    g_api.registerServerTick(&OnServerTick);
    if (g_api.registerEventHandler)
    {
        g_api.registerEventHandler("player.join", &OnAnyEvent);
        g_api.registerEventHandler("player.tick", &OnAnyEvent);
        g_api.registerEventHandler("player.use_item", &OnAnyEvent);
        g_api.registerEventHandler("block.place", &OnAnyEvent);
        g_api.registerEventHandler("block.break", &OnAnyEvent);
        g_api.registerEventHandler("entity.hurt", &OnAnyEvent);
        g_api.registerEventHandler("entity.death", &OnAnyEvent);
        g_api.registerEventHandler("world.tick", &OnAnyEvent);
        g_api.registerEventHandler("world.load", &OnAnyEvent);
        g_api.registerEventHandler("world.save", &OnAnyEvent);
        g_api.registerEventHandler("client.render_frame", &OnAnyEvent);
    }
    g_api.registerServerPacketHandler(&OnServerPacket);
    g_api.registerClientPacketHandler(&OnClientPacket);
    g_api.registerLevelLoad(&OnLevelLoad);
    g_api.registerLevelUnload(&OnLevelUnload);
    g_api.registerPlayerJoin(&OnPlayerJoin);
    g_api.registerPlayerLeave(&OnPlayerLeave);

    return true;
}

extern "C" __declspec(dllexport) void ShutdownMod()
{
    g_glowBlockId   = 0;
    g_crystalItemId = 0;
}
