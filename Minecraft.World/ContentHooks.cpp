#include "stdafx.h"

#include "ContentHooks.h"

void (*g_registerModTiles)(void) = nullptr;
void (*g_registerModItems)(void) = nullptr;
void (*g_registerModRecipes)(void) = nullptr;
void (*g_registerModTileEntities)(void) = nullptr;

void (*g_decorateChunk)(void* levelHandle,
    int chunkX,
    int chunkZ,
    unsigned int seed) = nullptr;

int (*g_hostCreateModTile)(int id,
    const wchar_t* textureName,
    int material,
    float hardness,
    float resistance,
    int lightEmission,
    int lightOpacity,
    int shape,
    int flags) = nullptr;

int (*g_hostCreateModItem)(int id,
    const wchar_t* textureName,
    int maxStack,
    int flags) = nullptr;

void (*g_hostAddShapedRecipe)(int outputId,
    int outputCount,
    int outputAux,
    int width,
    int height,
    const int* gridItems,
    const int* gridAux) = nullptr;

void (*g_hostAddShapelessRecipe)(int outputId,
    int outputCount,
    int outputAux,
    int count,
    const int* items,
    const int* auxVals) = nullptr;

void (*g_hostAddFurnaceRecipe)(int inputId,
    int outputId,
    int outputCount,
    int outputAux,
    float experience) = nullptr;

void (*g_worldGenPlaceBlock)(void* levelHandle,
    int x,
    int y,
    int z,
    int tileId,
    int data) = nullptr;

bool (*g_hostResolveStructurePlacementSender)(void* sender,
    void** outLevel,
    int* outX,
    int* outY,
    int* outZ) = nullptr;

bool (*g_hostSendModPacketToServer)(const wchar_t* channel,
    const void* payload,
    int size) = nullptr;

bool (*g_hostSendModPacketToClient)(void* player,
    const wchar_t* channel,
    const void* payload,
    int size) = nullptr;

int (*g_hostBroadcastModPacketToTracking)(void* level,
    int x,
    int y,
    int z,
    int radius,
    const wchar_t* channel,
    const void* payload,
    int size) = nullptr;

bool (*g_hostMarkModTileEntityDirty)(void* tileEntity) = nullptr;

// ---------------------------------------------------------------------------
// Player hand / offhand host operations
// ---------------------------------------------------------------------------

void* (*g_hostGetLocalPlayer)(void) = nullptr;

bool (*g_hostGetPlayerSelectedItem)(void* player, ModItemStack* out) = nullptr;
bool (*g_hostSetPlayerSelectedItem)(void* player, const ModItemStack* stack) = nullptr;

bool (*g_hostGetPlayerOffhandItem)(void* player, ModItemStack* out) = nullptr;
bool (*g_hostSetPlayerOffhandItem)(void* player, const ModItemStack* stack) = nullptr;

bool (*g_hostSwapPlayerHands)(void* player) = nullptr;

// ---------------------------------------------------------------------------

void (*g_hostRegisterModTileEntity)(const wchar_t* id, void* createFn) = nullptr;

bool (*g_executeModCommandText)(void* sender, const wchar_t* commandLine) = nullptr;

bool (*g_openModGuiForBlock)(int blockId,
    void* player,
    void* level,
    int x,
    int y,
    int z) = nullptr;

void (*g_modTileEntityOnSave)(const wchar_t* id,
    void* tileEntity,
    void* tag) = nullptr;

void (*g_modTileEntityOnLoad)(const wchar_t* id,
    void* tileEntity,
    void* tag) = nullptr;

bool (*g_modBlockOnUse)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    void* player,
    int clickedFace,
    float clickX,
    float clickY,
    float clickZ) = nullptr;

void (*g_modBlockOnBreak)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    void* player) = nullptr;

void (*g_modBlockOnPlaced)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    void* player,
    int itemAux) = nullptr;

void (*g_modBlockOnNeighborChanged)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    int neighborId) = nullptr;

void (*g_modBlockOnTick)(int tileId,
    void* level,
    int x,
    int y,
    int z,
    void* random) = nullptr;

bool (*g_modApplyBlockDrops)(int blockId,
    void* level,
    int x,
    int y,
    int z,
    int playerBonusLevel) = nullptr;

bool (*g_modApplyEntityDrops)(const wchar_t* entityId,
    void* entity,
    void* level,
    bool wasKilledByPlayer,
    int playerBonusLevel) = nullptr;

bool (*g_hostApplyBlockDropEntries)(void* level,
    int x,
    int y,
    int z,
    const ModDropEntryHost* entries,
    int entryCount,
    int playerBonusLevel) = nullptr;

bool (*g_hostApplyEntityDropEntries)(void* entity,
    void* level,
    const ModDropEntryHost* entries,
    int entryCount,
    bool wasKilledByPlayer,
    int playerBonusLevel) = nullptr;

int (*g_hostCollectModCreativeItems)(ModCreativeItemHost* outItems,
    int maxItems) = nullptr;

int (*g_hostCollectModCreativeCategories)(ModCreativeCategoryHost* outCategories,
    int maxCategories) = nullptr;