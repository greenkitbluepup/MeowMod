#include "ModLoader.h"
#include "..\\Minecraft.World\\LevelHooks.h"
#include "..\\Minecraft.World\\ChunkBakeHooks.h"
#include "..\\Minecraft.World\\EventHooks.h"
#include "..\\Minecraft.World\\ContentHooks.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cwctype>

ModLoader g_modLoader;

static void modLog(const char* tag, const char* message);

// ---------------------------------------------------------------------------
// Trampoline: bridges the C function-pointer in ModHostAPI to g_modLoader.
// Must be a plain static function so its address is stable.
// ---------------------------------------------------------------------------

static void RegisterClientTickThunk(ClientTickFn fn)
{
    g_modLoader.registerClientTick(fn);
}

void ModLoader::applyModTileEntities()
{
    for (const TileEntityEntry& te : m_tileEntities)
    {
        if (!g_hostRegisterModTileEntity || te.id.empty() || !te.create)
            continue;
        std::wstring wid(te.id.begin(), te.id.end());
        g_hostRegisterModTileEntity(wid.c_str(), reinterpret_cast<void*>(te.create));
        modLog("ModLoader", "Applied tile entity");
    }
}

static std::string narrowAscii(const std::wstring& w)
{
    std::string s;
    s.reserve(w.size());
    for (wchar_t c : w)
        s.push_back((c >= 0 && c <= 127) ? static_cast<char>(c) : '?');
    return s;
}

static std::wstring widenAscii(const char* s)
{
    std::wstring w;
    if (!s)
        return w;
    while (*s)
        w.push_back(static_cast<unsigned char>(*s++));
    return w;
}

static std::string bytesToHex(const unsigned char* data, int size)
{
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    if (!data || size <= 0)
        return out;
    out.reserve(static_cast<size_t>(size) * 2);
    for (int i = 0; i < size; ++i)
    {
        unsigned char b = data[i];
        out.push_back(kHex[(b >> 4) & 0xF]);
        out.push_back(kHex[b & 0xF]);
    }
    return out;
}

bool ModLoader::executeCommandText(void* sender, const wchar_t* commandLine) const
{
    emitEvent("commands.execute", sender, commandLine,
              commandLine ? static_cast<int>((wcslen(commandLine) + 1) * sizeof(wchar_t)) : 0);

    if (!commandLine)
        return false;

    std::wstring wline(commandLine);
    while (!wline.empty() && iswspace(wline.front()))
        wline.erase(wline.begin());
    if (!wline.empty() && wline.front() == L'/')
        wline.erase(wline.begin());
    while (!wline.empty() && iswspace(wline.front()))
        wline.erase(wline.begin());
    if (wline.empty())
        return false;

    size_t sp = wline.find(L' ');
    std::wstring wname = (sp == std::wstring::npos) ? wline : wline.substr(0, sp);
    std::wstring wargs = (sp == std::wstring::npos) ? L"" : wline.substr(sp + 1);

    std::string name = narrowAscii(wname);
    std::string args = narrowAscii(wargs);

    for (const CommandEntry& cmd : m_commands)
    {
        if (_stricmp(cmd.name.c_str(), name.c_str()) == 0 && cmd.execute)
            return cmd.execute(sender, args.c_str());
    }
    return false;
}

bool ModLoader::dispatchGuiOpenByBlock(int blockId,
                                       void* player,
                                       void* level,
                                       int x, int y, int z) const
{
    for (const GuiEntry& gui : m_guis)
    {
        if (gui.blockId == blockId && gui.open)
        {
            gui.open(player, level, x, y, z);
            return true;
        }
    }
    return false;
}

void ModLoader::dispatchTileEntitySave(const wchar_t* id, void* tileEntity, void* tag) const
{
    if (!id)
        return;
    std::string sid = narrowAscii(id);
    for (const TileEntityEntry& te : m_tileEntities)
    {
        if (_stricmp(te.id.c_str(), sid.c_str()) == 0 && te.onSave)
            te.onSave(tileEntity, tag);
    }
}

void ModLoader::dispatchTileEntityLoad(const wchar_t* id, void* tileEntity, void* tag) const
{
    if (!id)
        return;
    std::string sid = narrowAscii(id);
    for (const TileEntityEntry& te : m_tileEntities)
    {
        if (_stricmp(te.id.c_str(), sid.c_str()) == 0 && te.onLoad)
            te.onLoad(tileEntity, tag);
    }
}

static void RegisterDynamicLightQueryThunk(QueryDynamicLightFn fn)
{
    g_modLoader.registerDynamicLightQuery(fn);
}

static void RegisterPrepareChunkLightSnapshotThunk(PrepareChunkLightSnapshotFn fn)
{
    g_modLoader.registerPrepareChunkLightSnapshot(fn);
}

static void RegisterQueryChunkSnapshotLightThunk(QueryChunkSnapshotLightFn fn)
{
    g_modLoader.registerQueryChunkSnapshotLight(fn);
}

static void RegisterDestroyChunkLightSnapshotThunk(DestroyChunkLightSnapshotFn fn)
{
    g_modLoader.registerDestroyChunkLightSnapshot(fn);
}

static void RegisterBeginEmitterFeedThunk(BeginEmitterFeedFn fn)
{
    g_modLoader.registerBeginEmitterFeed(fn);
}

static void RegisterNotifyEmitterThunk(NotifyEmitterFn fn)
{
    g_modLoader.registerNotifyEmitter(fn);
}

static void RegisterEndEmitterFeedThunk(EndEmitterFeedFn fn)
{
    g_modLoader.registerEndEmitterFeed(fn);
}

static void RegisterTileChangedThunk(TileChangedFn fn)
{
    g_modLoader.registerTileChanged(fn);
}

static int RegisterBlockThunk(const ModBlockDef* def)
{
    return g_modLoader.registerBlock(def);
}

static int RegisterItemThunk(const ModItemDef* def)
{
    return g_modLoader.registerItem(def);
}

static void RegisterRecipeThunk(const ModRecipeDef* def)
{
    g_modLoader.registerRecipe(def);
}

static void RegisterShapedRecipeThunk(int outputId, int outputCount, int outputAux,
                                      int width, int height,
                                      const int* gridItems, const int* gridAux)
{
    g_modLoader.registerShapedRecipe(outputId, outputCount, outputAux,
                                     width, height, gridItems, gridAux);
}

static void RegisterShapelessRecipeThunk(int outputId, int outputCount, int outputAux,
                                         int count,
                                         const int* items, const int* auxVals)
{
    g_modLoader.registerShapelessRecipe(outputId, outputCount, outputAux,
                                        count, items, auxVals);
}

static void RegisterSmeltingRecipeThunk(int inputId, int inputAux,
                                        int outputId, int outputCount, int outputAux,
                                        float experience)
{
    g_modLoader.registerSmeltingRecipe(inputId, inputAux,
                                       outputId, outputCount, outputAux,
                                       experience);
}

static void RegisterBlockDropThunk(int blockId,
                                   const ModDropEntry* entries,
                                   int entryCount)
{
    g_modLoader.registerBlockDrop(blockId, entries, entryCount);
}

static void RegisterEntityDropThunk(const char* entityId,
                                    const ModDropEntry* entries,
                                    int entryCount)
{
    g_modLoader.registerEntityDrop(entityId, entries, entryCount);
}

static void RegisterLootTableThunk(const char* tableId,
                                   const ModDropEntry* entries,
                                   int entryCount)
{
    g_modLoader.registerLootTable(tableId, entries, entryCount);
}

static int RegisterCreativeCategoryThunk(const char* id,
                                         const char* displayName,
                                         int baseGroup)
{
    return g_modLoader.registerCreativeCategory(id, displayName, baseGroup);
}

static void RegisterCreativeItemThunk(const ModCreativeItem* def)
{
    g_modLoader.registerCreativeItem(def);
}

static bool LoadModJsonThunk(const char* relativePath)
{
    return g_modLoader.loadModJson(relativePath);
}

static void RegisterWorldGenThunk(const ModWorldGenDef* def)
{
    g_modLoader.registerWorldGen(def);
}

static void RegisterBlockCallbacksThunk(int blockId, const ModBlockCallbacks* callbacks)
{
    g_modLoader.registerBlockCallbacks(blockId, callbacks);
}

static void RegisterTileEntityThunk(const ModTileEntityDef* def)
{
    g_modLoader.registerTileEntity(def);
}

static void RegisterGuiThunk(const ModGuiDef* def)
{
    g_modLoader.registerGui(def);
}

static void RegisterCommandThunk(const ModCommandDef* def)
{
    g_modLoader.registerCommand(def);
}

static void RegisterStructureThunk(const ModStructureDef* def)
{
    g_modLoader.registerStructure(def);
}

static void RegisterStructureTemplateThunk(const ModStructureTemplateDef* def)
{
    g_modLoader.registerStructureTemplate(def);
}

static void RegisterProjectileThunk(const ModProjectileDef* def)
{
    g_modLoader.registerProjectile(def);
}

static void RegisterServerPacketHandlerThunk(ModServerPacketHandlerFn fn)
{
    g_modLoader.registerServerPacketHandler(fn);
}

static void RegisterClientPacketHandlerThunk(ModClientPacketHandlerFn fn)
{
    g_modLoader.registerClientPacketHandler(fn);
}

static void* SpawnProjectileThunk(const char* id,
                                  void* level,
                                  void* owner,
                                  float x, float y, float z,
                                  float vx, float vy, float vz)
{
    return g_modLoader.spawnProjectile(id, level, owner, x, y, z, vx, vy, vz);
}

static bool PlaceStructureThunk(void* level,
                                const char* templateId,
                                int originX, int originY, int originZ,
                                int rotation)
{
    return g_modLoader.placeStructure(level, templateId, originX, originY, originZ, rotation);
}

static bool PlaceStructureFromSenderThunk(void* sender,
                                          const char* templateId,
                                          int offsetX, int offsetY, int offsetZ,
                                          int rotation)
{
    if (!sender || !g_hostResolveStructurePlacementSender)
        return false;

    void* level = nullptr;
    int baseX = 0;
    int baseY = 0;
    int baseZ = 0;
    if (!g_hostResolveStructurePlacementSender(sender, &level, &baseX, &baseY, &baseZ))
        return false;

    return g_modLoader.placeStructure(level,
                                      templateId,
                                      baseX + offsetX,
                                      baseY + offsetY,
                                      baseZ + offsetZ,
                                      rotation);
}

static bool SendToServerThunk(const char* channel, const void* payload, int size)
{
    return g_modLoader.sendToServer(channel, payload, size);
}

static bool SendToClientThunk(void* player, const char* channel, const void* payload, int size)
{
    return g_modLoader.sendToClient(player, channel, payload, size);
}

static int BroadcastToTrackingThunk(void* level,
                                    int x, int y, int z, int radius,
                                    const char* channel,
                                    const void* payload,
                                    int size)
{
    return g_modLoader.broadcastToTracking(level, x, y, z, radius, channel, payload, size);
}

static bool MarkModTileEntityDirtyThunk(void* tileEntity)
{
    if (!g_hostMarkModTileEntityDirty)
        return false;
    return g_hostMarkModTileEntityDirty(tileEntity);
}

static void RegisterEntityThunk(const ModEntityDef* def)
{
    g_modLoader.registerEntity(def);
}

static void* SpawnEntityThunk(const char* id,
                              void* level,
                              float x, float y, float z)
{
    return g_modLoader.spawnEntity(id, level, x, y, z);
}

static void RegisterTextureThunk(const char* id, const char* path)
{
    g_modLoader.registerTexture(id, path);
}

static void RegisterSoundThunk(const char* id, const char* path)
{
    g_modLoader.registerSound(id, path);
}

static void RegisterLangThunk(const char* id, const char* path)
{
    g_modLoader.registerLang(id, path);
}

static void RegisterModelThunk(const char* id, const char* path)
{
    g_modLoader.registerModel(id, path);
}

static bool LoadModAssetsThunk(const char* manifestRelativePath)
{
    return g_modLoader.loadModAssets(manifestRelativePath);
}

static void RegisterBlockRendererThunk(const ModBlockRendererDef* def)
{
    g_modLoader.registerBlockRenderer(def);
}

static void RegisterItemRendererThunk(const ModItemRendererDef* def)
{
    g_modLoader.registerItemRenderer(def);
}

static void RegisterEntityRendererThunk(const ModEntityRendererDef* def)
{
    g_modLoader.registerEntityRenderer(def);
}

static void RegisterProjectileRendererThunk(const ModProjectileRendererDef* def)
{
    g_modLoader.registerProjectileRenderer(def);
}

static void RegisterEventHandlerThunk(const char* eventName, ModEventHandlerFn fn)
{
    g_modLoader.registerEventHandler(eventName, fn);
}

static void EmitEventThunk(const ModEvent* e)
{
    g_modLoader.emitEvent(e);
}

static void RegisterServerTickThunk(ServerTickFn fn)
{
    g_modLoader.registerServerTick(fn);
}

static void RegisterLevelLoadThunk(LevelLoadFn fn)
{
    g_modLoader.registerLevelLoad(fn);
}

static void RegisterLevelUnloadThunk(LevelUnloadFn fn)
{
    g_modLoader.registerLevelUnload(fn);
}

static void RegisterPlayerJoinThunk(PlayerJoinFn fn)
{
    g_modLoader.registerPlayerJoin(fn);
}

static void RegisterPlayerLeaveThunk(PlayerLeaveFn fn)
{
    g_modLoader.registerPlayerLeave(fn);
}

static const char* GetConfigThunk(ModConfig* cfg, const char* key, const char* defaultVal)
{
    if (!cfg || !key)
        return defaultVal;
    auto it = cfg->values.find(key);
    return (it != cfg->values.end()) ? it->second.c_str() : defaultVal;
}

// ---------------------------------------------------------------------------
// Player hand / offhand API thunks
// ---------------------------------------------------------------------------

static void* GetLocalPlayerThunk()
{
    return g_hostGetLocalPlayer
        ? g_hostGetLocalPlayer()
        : nullptr;
}

static bool GetPlayerSelectedItemThunk(void* player, ModItemStack* out)
{
    return g_hostGetPlayerSelectedItem
        ? g_hostGetPlayerSelectedItem(player, out)
        : false;
}

static bool SetPlayerSelectedItemThunk(void* player, const ModItemStack* stack)
{
    return g_hostSetPlayerSelectedItem
        ? g_hostSetPlayerSelectedItem(player, stack)
        : false;
}

static bool GetPlayerOffhandItemThunk(void* player, ModItemStack* out)
{
    return g_hostGetPlayerOffhandItem
        ? g_hostGetPlayerOffhandItem(player, out)
        : false;
}

static bool SetPlayerOffhandItemThunk(void* player, const ModItemStack* stack)
{
    return g_hostSetPlayerOffhandItem
        ? g_hostSetPlayerOffhandItem(player, stack)
        : false;
}

static bool SwapPlayerHandsThunk(void* player)
{
    return g_hostSwapPlayerHands
        ? g_hostSwapPlayerHands(player)
        : false;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void modLog(const char* tag, const char* message)
{
    char buf[1024];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "[%s] %s\n", tag, message);
    OutputDebugStringA(buf);
}

void ModLoader::hostLog(const char* message)
{
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

bool ModLoader::requirePermission(const char* permission, const char* apiName) const
{
    if (!permission || !*permission)
        return true;

    if (m_activePermissionModId.empty())
        return true;

    std::string modKey = m_activePermissionModId;
    std::transform(modKey.begin(), modKey.end(), modKey.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string permKey = permission;
    std::transform(permKey.begin(), permKey.end(), permKey.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto it = m_permissionsByMod.find(modKey);
    bool granted = (it != m_permissionsByMod.end() && it->second.find(permKey) != it->second.end());
    if (granted)
        return true;

    char msg[768];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE,
        "Permission '%s' not declared for mod '%s' while calling %s (%s mode)",
        permission,
        m_activePermissionModId.c_str(),
        apiName ? apiName : "api",
        m_permissionsStrict ? "strict" : "warn-only");
    modLog("ModLoader", msg);

    return !m_permissionsStrict;
}

// ---------------------------------------------------------------------------
// Minimal mod.json parser
// Supports only flat string fields: "key": "value"
// ---------------------------------------------------------------------------

static std::string jsonReadField(const std::string& src, const char* key)
{
    // Search for "key"
    std::string needle = std::string("\"") + key + "\"";
    auto pos = src.find(needle);
    if (pos == std::string::npos)
        return {};
    pos += needle.size();
    // Skip whitespace and colon
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == ':' || src[pos] == '\r' || src[pos] == '\n'))
        ++pos;
    if (pos >= src.size() || src[pos] != '\"')
        return {};
    ++pos; // skip opening quote
    std::string result;
    while (pos < src.size() && src[pos] != '\"')
    {
        if (src[pos] == '\\' && pos + 1 < src.size())
            ++pos; // skip escape prefix, take next char
        result += src[pos++];
    }
    return result;
}

static bool jsonReadRawValue(const std::string& src, const char* key, std::string& outValue)
{
    std::string needle = std::string("\"") + key + "\"";
    std::size_t pos = src.find(needle);
    if (pos == std::string::npos)
        return false;
    pos += needle.size();

    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n'))
        ++pos;
    if (pos >= src.size() || src[pos] != ':')
        return false;
    ++pos;
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n'))
        ++pos;
    if (pos >= src.size())
        return false;

    std::size_t start = pos;
    if (src[pos] == '"')
    {
        ++pos;
        while (pos < src.size())
        {
            if (src[pos] == '\\' && (pos + 1) < src.size())
            {
                pos += 2;
                continue;
            }
            if (src[pos] == '"')
            {
                ++pos;
                break;
            }
            ++pos;
        }
        outValue = src.substr(start, pos - start);
        return true;
    }

    if (src[pos] == '[')
    {
        int depth = 1;
        ++pos;
        while (pos < src.size() && depth > 0)
        {
            if (src[pos] == '[') ++depth;
            else if (src[pos] == ']') --depth;
            ++pos;
        }
        outValue = src.substr(start, pos - start);
        return true;
    }

    while (pos < src.size() && src[pos] != ',' && src[pos] != '}' && src[pos] != '\r' && src[pos] != '\n')
        ++pos;
    outValue = src.substr(start, pos - start);
    return true;
}

static bool parseStringArrayField(const std::string& src,
                                  const char* key,
                                  std::vector<std::string>& out)
{
    out.clear();
    std::string raw;
    if (!jsonReadRawValue(src, key, raw))
        return true;

    std::size_t i = raw.find('[');
    if (i == std::string::npos)
        return true;
    ++i;

    while (i < raw.size())
    {
        while (i < raw.size() && (std::isspace(static_cast<unsigned char>(raw[i])) || raw[i] == ',')) ++i;
        if (i >= raw.size() || raw[i] == ']')
            break;
        if (raw[i] != '"')
            return false;
        ++i;

        std::string s;
        while (i < raw.size())
        {
            char c = raw[i++];
            if (c == '\\' && i < raw.size())
            {
                s.push_back(raw[i++]);
                continue;
            }
            if (c == '"')
                break;
            s.push_back(c);
        }
        if (!s.empty())
            out.push_back(s);
    }

    return true;
}

static bool jsonReadIntField(const std::string& src, const char* key, int* outValue)
{
    if (!outValue)
        return false;
    std::string raw;
    if (!jsonReadRawValue(src, key, raw))
        return false;
    char* endPtr = nullptr;
    long v = strtol(raw.c_str(), &endPtr, 10);
    if (endPtr == raw.c_str())
        return false;
    *outValue = static_cast<int>(v);
    return true;
}

static bool jsonReadFloatField(const std::string& src, const char* key, float* outValue)
{
    if (!outValue)
        return false;
    std::string raw;
    if (!jsonReadRawValue(src, key, raw))
        return false;
    char* endPtr = nullptr;
    float v = strtof(raw.c_str(), &endPtr);
    if (endPtr == raw.c_str())
        return false;
    *outValue = v;
    return true;
}

static bool jsonReadIntArrayField(const std::string& src, const char* key, std::vector<int>& out)
{
    out.clear();
    std::string raw;
    if (!jsonReadRawValue(src, key, raw))
        return false;
    std::size_t i = 0;
    while (i < raw.size() && raw[i] != '[') ++i;
    if (i >= raw.size())
        return false;
    ++i;

    while (i < raw.size())
    {
        while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r' || raw[i] == '\n' || raw[i] == ',')) ++i;
        if (i >= raw.size() || raw[i] == ']')
            break;

        char* endPtr = nullptr;
        long v = strtol(raw.c_str() + i, &endPtr, 10);
        if (endPtr == raw.c_str() + i)
            return false;
        out.push_back(static_cast<int>(v));
        i = static_cast<std::size_t>(endPtr - raw.c_str());
    }

    return true;
}

static bool jsonSplitTopLevelArrayObjects(const std::string& src, std::vector<std::string>& outObjects)
{
    outObjects.clear();

    std::size_t i = src.find('[');
    if (i == std::string::npos)
        return false;
    ++i;

    while (i < src.size())
    {
        while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i]))) ++i;
        if (i >= src.size() || src[i] == ']')
            break;
        if (src[i] == ',')
        {
            ++i;
            continue;
        }
        if (src[i] != '{')
        {
            ++i;
            continue;
        }

        std::size_t start = i;
        int depth = 1;
        ++i;
        while (i < src.size() && depth > 0)
        {
            if (src[i] == '"')
            {
                ++i;
                while (i < src.size())
                {
                    if (src[i] == '\\' && (i + 1) < src.size())
                    {
                        i += 2;
                        continue;
                    }
                    if (src[i] == '"')
                    {
                        ++i;
                        break;
                    }
                    ++i;
                }
                continue;
            }

            if (src[i] == '{') ++depth;
            else if (src[i] == '}') --depth;
            ++i;
        }

        if (depth == 0)
            outObjects.push_back(src.substr(start, i - start));
    }

    return !outObjects.empty();
}

static std::string fileStemNoExt(const std::string& path)
{
    std::size_t slash = path.find_last_of("\\/");
    std::string file = (slash == std::string::npos) ? path : path.substr(slash + 1);
    std::size_t dot = file.rfind('.');
    if (dot != std::string::npos)
        file = file.substr(0, dot);
    return file;
}

static bool parseDependencyArray(const std::string& src,
                                 const char* key,
                                 std::vector<LoadedMod::DependencySpec>& out)
{
    out.clear();

    std::string raw;
    if (!jsonReadRawValue(src, key, raw))
        return true;

    std::vector<std::string> objs;
    if (!jsonSplitTopLevelArrayObjects(raw, objs))
        return true;

    for (const std::string& o : objs)
    {
        LoadedMod::DependencySpec d;
        d.id = jsonReadField(o, "id");
        d.version = jsonReadField(o, "version");
        if (!d.id.empty())
            out.push_back(d);
    }

    return true;
}

static int compareVersionStrings(const std::string& a, const std::string& b)
{
    auto parseParts = [](const std::string& s) -> std::vector<int>
    {
        std::vector<int> out;
        std::size_t i = 0;
        while (i < s.size())
        {
            while (i < s.size() && (s[i] == '.' || s[i] == ' ' || s[i] == '\t')) ++i;
            if (i >= s.size()) break;

            int v = 0;
            bool any = false;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
            {
                v = v * 10 + (s[i] - '0');
                ++i;
                any = true;
            }
            if (any) out.push_back(v);

            while (i < s.size() && s[i] != '.') ++i;
        }
        return out;
    };

    std::vector<int> pa = parseParts(a);
    std::vector<int> pb = parseParts(b);
    std::size_t n = (pa.size() > pb.size()) ? pa.size() : pb.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        int va = (i < pa.size()) ? pa[i] : 0;
        int vb = (i < pb.size()) ? pb[i] : 0;
        if (va < vb) return -1;
        if (va > vb) return 1;
    }
    return 0;
}

static bool versionSatisfiesConstraint(const std::string& version, const std::string& constraint)
{
    std::string c = constraint;
    c.erase(0, c.find_first_not_of(" \t"));
    if (c.empty() || c == "*")
        return true;

    auto starts = [&](const char* p) -> bool { return c.rfind(p, 0) == 0; };
    auto rhs = [&](std::size_t n) -> std::string
    {
        std::string s = c.substr(n);
        std::size_t p = s.find_first_not_of(" \t");
        if (p != std::string::npos) s = s.substr(p);
        return s;
    };

    if (starts(">=")) return compareVersionStrings(version, rhs(2)) >= 0;
    if (starts("<=")) return compareVersionStrings(version, rhs(2)) <= 0;
    if (starts("==")) return compareVersionStrings(version, rhs(2)) == 0;
    if (starts(">"))  return compareVersionStrings(version, rhs(1)) > 0;
    if (starts("<"))  return compareVersionStrings(version, rhs(1)) < 0;
    if (starts("="))  return compareVersionStrings(version, rhs(1)) == 0;

    return compareVersionStrings(version, c) == 0;
}

static bool tryParseModJson(const char* dllPath, LoadedMod& out)
{
    // Replace .dll extension with .json
    std::string jsonPath = dllPath;
    auto dot = jsonPath.rfind('.');
    if (dot != std::string::npos)
        jsonPath = jsonPath.substr(0, dot);
    jsonPath += ".json";

    std::ifstream f(jsonPath);
    if (!f.is_open())
        return false;

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    out.modId       = jsonReadField(src, "id");
    out.displayName = jsonReadField(src, "name");
    out.version     = jsonReadField(src, "version");
    out.description = jsonReadField(src, "description");
    out.author      = jsonReadField(src, "author");
    parseDependencyArray(src, "requires", out.requires);
    parseDependencyArray(src, "optional", out.optional);
    parseDependencyArray(src, "conflicts", out.conflicts);
    parseStringArrayField(src, "permissions", out.permissions);
    return true;
}

// ---------------------------------------------------------------------------
// Config parser
// Format: flat key=value lines; leading/trailing whitespace stripped;
// blank lines and lines starting with '#' are ignored.
// File path: replace .dll extension with .cfg.
// ---------------------------------------------------------------------------

static ModConfig* tryLoadModConfig(const char* dllPath)
{
    std::string cfgPath = dllPath;
    auto dot = cfgPath.rfind('.');
    if (dot != std::string::npos)
        cfgPath = cfgPath.substr(0, dot);
    cfgPath += ".cfg";

    std::ifstream f(cfgPath);
    if (!f.is_open())
        return nullptr;

    ModConfig* cfg = new ModConfig();
    std::string line;
    while (std::getline(f, line))
    {
        // Strip trailing carriage-return
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip blank lines and comments
        std::size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] == '#')
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(start, eq - start);
        std::string val = line.substr(eq + 1);

        // Trim trailing whitespace from key
        auto kend = key.find_last_not_of(" \t");
        if (kend != std::string::npos) key = key.substr(0, kend + 1);

        // Trim leading whitespace from value
        auto vstart = val.find_first_not_of(" \t");
        if (vstart != std::string::npos) val = val.substr(vstart);

        if (!key.empty())
            cfg->values[key] = val;
    }
    return cfg;
}

static std::string toLowerAscii(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

static bool readTextFile(const std::string& path, std::string& out)
{
    std::ifstream f(path);
    if (!f.is_open())
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static bool parseDropEntriesJson(const std::string& rawArray, std::vector<ModDropEntry>& out)
{
    out.clear();
    std::vector<std::string> objs;
    if (!jsonSplitTopLevelArrayObjects(rawArray, objs))
        return false;

    for (const std::string& o : objs)
    {
        ModDropEntry d{};
        if (!jsonReadIntField(o, "itemId", &d.itemId))
            continue;
        if (!jsonReadIntField(o, "aux", &d.aux)) d.aux = 0;
        if (!jsonReadIntField(o, "minCount", &d.minCount)) d.minCount = 1;
        if (!jsonReadIntField(o, "maxCount", &d.maxCount)) d.maxCount = d.minCount;
        if (!jsonReadFloatField(o, "chance", &d.chance)) d.chance = 1.0f;
        out.push_back(d);
    }

    return !out.empty();
}

static bool parseStringArrayJson(const std::string& rawArray, std::vector<std::string>& out)
{
    out.clear();
    std::size_t i = rawArray.find('[');
    if (i == std::string::npos)
        return false;
    ++i;

    while (i < rawArray.size())
    {
        while (i < rawArray.size() && (std::isspace(static_cast<unsigned char>(rawArray[i])) || rawArray[i] == ','))
            ++i;
        if (i >= rawArray.size() || rawArray[i] == ']')
            break;
        if (rawArray[i] != '"')
            return false;

        ++i;
        std::string s;
        while (i < rawArray.size())
        {
            char c = rawArray[i++];
            if (c == '\\' && i < rawArray.size())
            {
                s.push_back(rawArray[i++]);
                continue;
            }
            if (c == '"')
                break;
            s.push_back(c);
        }
        if (!s.empty())
            out.push_back(s);
    }

    return true;
}

// ---------------------------------------------------------------------------
// loadAllMods
// ---------------------------------------------------------------------------

void ModLoader::loadAllMods(const char* modsDir)
{
    m_permissionsByMod.clear();
    m_activePermissionModId.clear();

    // -----------------------------------------------------------------------
    // 1. Collect all DLL filenames and sort them lexically so load order is
    //    deterministic regardless of filesystem enumeration order.
    // -----------------------------------------------------------------------
    char pattern[MAX_PATH];
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*.dll", modsDir);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern, &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        modLog("ModLoader", "No mods folder found or no .dll files present");
        return;
    }

    std::vector<std::string> dllNames;
    do
    {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            dllNames.push_back(findData.cFileName);
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);

    std::sort(dllNames.begin(), dllNames.end());

    struct ModCandidate
    {
        std::string dllName;
        std::string fullPath;
        std::string modDir;
        LoadedMod   entry;
        bool        rejected = false;
        std::string rejectReason;
    };

    std::vector<ModCandidate> candidates;
    candidates.reserve(dllNames.size());

    int found = static_cast<int>(dllNames.size());

    for (const std::string& dllName : dllNames)
    {
        ModCandidate c;
        c.dllName = dllName;
        c.fullPath = std::string(modsDir) + "\\" + dllName;
        std::size_t slash = c.fullPath.find_last_of("\\/");
        c.modDir = (slash == std::string::npos) ? std::string(modsDir) : c.fullPath.substr(0, slash);

        c.entry.name = dllName;
        tryParseModJson(c.fullPath.c_str(), c.entry);
        if (c.entry.modId.empty())
            c.entry.modId = fileStemNoExt(dllName);
        if (c.entry.version.empty())
            c.entry.version = "0.0.0";

        candidates.push_back(c);
    }

    std::unordered_map<std::string, int> modIdToIndex;
    for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
    {
        std::string key = toLowerAscii(candidates[i].entry.modId);
        auto it = modIdToIndex.find(key);
        if (it != modIdToIndex.end())
        {
            candidates[i].rejected = true;
            candidates[i].rejectReason = "duplicate mod id: " + candidates[i].entry.modId;
            candidates[it->second].rejected = true;
            candidates[it->second].rejectReason = "duplicate mod id: " + candidates[it->second].entry.modId;
            continue;
        }
        modIdToIndex[key] = i;
    }

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
    {
        if (candidates[i].rejected)
            continue;

        const LoadedMod& mod = candidates[i].entry;

        for (const auto& d : mod.requires)
        {
            auto it = modIdToIndex.find(toLowerAscii(d.id));
            if (it == modIdToIndex.end() || candidates[it->second].rejected)
            {
                candidates[i].rejected = true;
                candidates[i].rejectReason = "missing required dependency: " + d.id;
                break;
            }
            const LoadedMod& dep = candidates[it->second].entry;
            if (!versionSatisfiesConstraint(dep.version, d.version))
            {
                candidates[i].rejected = true;
                candidates[i].rejectReason = "required dependency version mismatch: " + d.id + " " + d.version + " (found " + dep.version + ")";
                break;
            }
        }

        if (candidates[i].rejected)
            continue;

        for (const auto& d : mod.optional)
        {
            auto it = modIdToIndex.find(toLowerAscii(d.id));
            if (it == modIdToIndex.end() || candidates[it->second].rejected)
            {
                char msg[512];
                _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                    "Optional dependency missing for %s: %s", mod.modId.c_str(), d.id.c_str());
                modLog("ModLoader", msg);
                continue;
            }

            const LoadedMod& dep = candidates[it->second].entry;
            if (!versionSatisfiesConstraint(dep.version, d.version))
            {
                char msg[512];
                _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                    "Optional dependency version mismatch for %s: %s %s (found %s)",
                    mod.modId.c_str(), d.id.c_str(), d.version.c_str(), dep.version.c_str());
                modLog("ModLoader", msg);
            }
        }

        for (const auto& d : mod.conflicts)
        {
            auto it = modIdToIndex.find(toLowerAscii(d.id));
            if (it == modIdToIndex.end() || candidates[it->second].rejected)
                continue;

            const LoadedMod& conflict = candidates[it->second].entry;
            if (versionSatisfiesConstraint(conflict.version, d.version))
            {
                candidates[i].rejected = true;
                candidates[i].rejectReason = "conflict with " + d.id + " " + d.version;
                break;
            }
        }
    }

    std::vector<int> orderedIndices;
    std::vector<int> indegree(candidates.size(), 0);
    std::vector<std::vector<int>> outgoing(candidates.size());
    std::vector<bool> active(candidates.size(), false);

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
    {
        if (!candidates[i].rejected)
            active[i] = true;
    }

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
    {
        if (!active[i]) continue;

        const LoadedMod& mod = candidates[i].entry;
        for (const auto& d : mod.requires)
        {
            auto it = modIdToIndex.find(toLowerAscii(d.id));
            if (it == modIdToIndex.end()) continue;
            int depIdx = it->second;
            if (!active[depIdx]) continue;
            outgoing[depIdx].push_back(i);
            ++indegree[i];
        }

        for (const auto& d : mod.optional)
        {
            auto it = modIdToIndex.find(toLowerAscii(d.id));
            if (it == modIdToIndex.end()) continue;
            int depIdx = it->second;
            if (!active[depIdx]) continue;
            if (!versionSatisfiesConstraint(candidates[depIdx].entry.version, d.version))
                continue;
            outgoing[depIdx].push_back(i);
            ++indegree[i];
        }
    }

    std::vector<bool> emitted(candidates.size(), false);
    while (true)
    {
        int pick = -1;
        for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
        {
            if (!active[i] || emitted[i] || indegree[i] != 0)
                continue;
            if (pick < 0 || _stricmp(candidates[i].entry.modId.c_str(), candidates[pick].entry.modId.c_str()) < 0)
                pick = i;
        }

        if (pick < 0)
            break;

        emitted[pick] = true;
        orderedIndices.push_back(pick);
        for (int n : outgoing[pick])
            --indegree[n];
    }

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
    {
        if (active[i] && !emitted[i])
        {
            candidates[i].rejected = true;
            candidates[i].rejectReason = "dependency cycle detected";
        }
    }

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
    {
        if (candidates[i].rejected)
        {
            char msg[768];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "Skipping %s (%s): %s",
                candidates[i].dllName.c_str(),
                candidates[i].entry.modId.c_str(),
                candidates[i].rejectReason.c_str());
            modLog("ModLoader", msg);
        }
    }

    // -----------------------------------------------------------------------
    // 2. Load each accepted DLL in dependency-resolved order.
    // -----------------------------------------------------------------------
    for (int idx : orderedIndices)
    {
        ModCandidate& c = candidates[idx];

        m_currentModDir = c.modDir;
        m_loadedJsonPathsForCurrentMod.clear();

        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Loading %s (%s %s)", c.dllName.c_str(), c.entry.modId.c_str(), c.entry.version.c_str());
            modLog("ModLoader", msg);
        }

        HMODULE hMod = LoadLibraryA(c.fullPath.c_str());
        if (!hMod)
        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Failed to load %s (error %lu)", c.dllName.c_str(), GetLastError());
            modLog("ModLoader", msg);
            m_currentModDir.clear();
            m_loadedJsonPathsForCurrentMod.clear();
            continue;
        }

        InitModFn initFn = reinterpret_cast<InitModFn>(GetProcAddress(hMod, "InitMod"));
        if (!initFn)
        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "%s has no InitMod export – skipping", c.dllName.c_str());
            modLog("ModLoader", msg);
            FreeLibrary(hMod);
            m_currentModDir.clear();
            m_loadedJsonPathsForCurrentMod.clear();
            continue;
        }

        ShutdownModFn shutdownFn = reinterpret_cast<ShutdownModFn>(GetProcAddress(hMod, "ShutdownMod"));

        LoadedMod entry = c.entry;
        entry.handle = static_cast<void*>(hMod);
        entry.shutdownFn = shutdownFn;

        std::unordered_set<std::string> granted;
        for (const std::string& p : entry.permissions)
            granted.insert(toLowerAscii(p));
        m_permissionsByMod[toLowerAscii(entry.modId)] = granted;
        m_activePermissionModId = entry.modId;

        // -----------------------------------------------------------------------
        // 3. Load config file (<modid>.cfg or <dllname>.cfg beside the DLL).
        // -----------------------------------------------------------------------
        entry.config = tryLoadModConfig(c.fullPath.c_str());

        // Build a temporary ModMetadata pointing into the entry strings.
        ModMetadata meta{};
        meta.id          = entry.modId.empty()       ? c.dllName.c_str() : entry.modId.c_str();
        meta.name        = entry.displayName.empty() ? c.dllName.c_str() : entry.displayName.c_str();
        meta.version     = entry.version.c_str();
        meta.description = entry.description.c_str();
        meta.author      = entry.author.c_str();

        // Build the host API struct. Value-initialize ({}) so every slot is
        // zero before we fill in the fields we know about — prevents garbage
        // function pointers reaching the mod if the struct ever grows.
        ModHostAPI api{};
        api.apiVersion                        = MOD_API_VERSION;
        api.metadata                          = &meta;
        api.config                            = entry.config;
        api.getConfig                         = &GetConfigThunk;
        api.log                               = &ModLoader::hostLog;
        api.registerClientTick                = &RegisterClientTickThunk;
        api.registerDynamicLightQuery         = &RegisterDynamicLightQueryThunk;
        api.registerPrepareChunkLightSnapshot = &RegisterPrepareChunkLightSnapshotThunk;
        api.registerQueryChunkSnapshotLight   = &RegisterQueryChunkSnapshotLightThunk;
        api.registerDestroyChunkLightSnapshot = &RegisterDestroyChunkLightSnapshotThunk;
        api.registerBlock                     = &RegisterBlockThunk;
        api.registerItem                      = &RegisterItemThunk;
        api.registerEntity                    = &RegisterEntityThunk;
        api.spawnEntity                       = &SpawnEntityThunk;
        api.registerTexture                   = &RegisterTextureThunk;
        api.registerSound                     = &RegisterSoundThunk;
        api.registerLang                      = &RegisterLangThunk;
        api.registerModel                     = &RegisterModelThunk;
        api.registerRecipe                    = &RegisterRecipeThunk;
        api.registerShapedRecipe              = &RegisterShapedRecipeThunk;
        api.registerShapelessRecipe           = &RegisterShapelessRecipeThunk;
        api.registerSmeltingRecipe            = &RegisterSmeltingRecipeThunk;
        api.registerBlockDrop                 = &RegisterBlockDropThunk;
        api.registerEntityDrop                = &RegisterEntityDropThunk;
        api.registerLootTable                 = &RegisterLootTableThunk;
        api.registerCreativeCategory          = &RegisterCreativeCategoryThunk;
        api.registerCreativeItem              = &RegisterCreativeItemThunk;
        api.loadModJson                       = &LoadModJsonThunk;
        api.loadModAssets                     = &LoadModAssetsThunk;
        api.registerBlockRenderer             = &RegisterBlockRendererThunk;
        api.registerItemRenderer              = &RegisterItemRendererThunk;
        api.registerEntityRenderer            = &RegisterEntityRendererThunk;
        api.registerProjectileRenderer        = &RegisterProjectileRendererThunk;
        api.registerEventHandler              = &RegisterEventHandlerThunk;
        api.emitEvent                         = &EmitEventThunk;
        api.registerWorldGen                  = &RegisterWorldGenThunk;
        api.registerBlockCallbacks            = &RegisterBlockCallbacksThunk;
        api.registerTileEntity                = &RegisterTileEntityThunk;
        api.registerGui                       = &RegisterGuiThunk;
        api.registerCommand                   = &RegisterCommandThunk;
        api.registerStructure                 = &RegisterStructureThunk;
        api.registerStructureTemplate         = &RegisterStructureTemplateThunk;
        api.registerProjectile                = &RegisterProjectileThunk;
        api.registerServerPacketHandler       = &RegisterServerPacketHandlerThunk;
        api.registerClientPacketHandler       = &RegisterClientPacketHandlerThunk;
        api.sendToServer                      = &SendToServerThunk;
        api.sendToClient                      = &SendToClientThunk;
        api.broadcastToTracking               = &BroadcastToTrackingThunk;
        api.placeStructure                    = &PlaceStructureThunk;
        api.placeStructureFromSender          = &PlaceStructureFromSenderThunk;
        api.markModTileEntityDirty            = &MarkModTileEntityDirtyThunk;
        api.spawnProjectile                   = &SpawnProjectileThunk;
        api.registerServerTick                = &RegisterServerTickThunk;
        api.registerLevelLoad                 = &RegisterLevelLoadThunk;
        api.registerLevelUnload               = &RegisterLevelUnloadThunk;
        api.registerPlayerJoin                = &RegisterPlayerJoinThunk;
        api.registerPlayerLeave               = &RegisterPlayerLeaveThunk;
        api.getTileOpacity                    = [](int x, int y, int z) -> int
        {
            return g_hostGetTileOpacity ? g_hostGetTileOpacity(x, y, z) : 0;
        };
        api.markRegionDirty                   = [](int x0, int y0, int z0, int x1, int y1, int z1)
        {
            if (g_markRegionDirty) g_markRegionDirty(x0, y0, z0, x1, y1, z1);
        };
        api.registerBeginEmitterFeed = &RegisterBeginEmitterFeedThunk;
        api.registerNotifyEmitter = &RegisterNotifyEmitterThunk;
        api.registerEndEmitterFeed = &RegisterEndEmitterFeedThunk;
        api.registerTileChanged = &RegisterTileChangedThunk;

        api.getLocalPlayer = &GetLocalPlayerThunk;

        api.getPlayerSelectedItem = &GetPlayerSelectedItemThunk;
        api.setPlayerSelectedItem = &SetPlayerSelectedItemThunk;

        api.getPlayerOffhandItem = &GetPlayerOffhandItemThunk;
        api.setPlayerOffhandItem = &SetPlayerOffhandItemThunk;

        api.swapPlayerHands = &SwapPlayerHandsThunk;

        bool ok = initFn(&api);
        m_activePermissionModId.clear();
        if (!ok)
        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "%s InitMod returned false – skipping", c.dllName.c_str());
            modLog("ModLoader", msg);
            FreeLibrary(hMod);
            m_currentModDir.clear();
            m_loadedJsonPathsForCurrentMod.clear();
            continue;
        }

        autoScanModContentJson(m_currentModDir);

        m_mods.push_back(std::move(entry));

        {
            char msg[512];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Successfully loaded %s", c.dllName.c_str());
            modLog("ModLoader", msg);
        }

        m_currentModDir.clear();
        m_loadedJsonPathsForCurrentMod.clear();
    }

    // -----------------------------------------------------------------------
    // 4. Summary log
    // -----------------------------------------------------------------------
    {
        char msg[128];
        _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Found %d mod(s), loaded %zu", found, m_mods.size());
        modLog("ModLoader", msg);
    }

    // -----------------------------------------------------------------------
    // 5. Publish engine hook slots.
    //    Dynamic light: any registered providers ? enable slot.
    // -----------------------------------------------------------------------
    if (!m_dynamicLightQueries.empty())
        g_queryDynamicLight = [](int x, int y, int z) -> int { return g_modLoader.queryDynamicLight(x, y, z); };
    else
        g_queryDynamicLight = nullptr;

    if (m_prepareChunkLightSnapshot)
        g_prepareChunkLightSnapshot = [](int cx, int cy, int cz) -> void* { return g_modLoader.prepareChunkLightSnapshot(cx, cy, cz); };
    else
        g_prepareChunkLightSnapshot = nullptr;

    if (m_queryChunkSnapshotLight)
        g_queryChunkSnapshotLight = [](void* s, int x, int y, int z) -> int { return g_modLoader.queryChunkSnapshotLight(s, x, y, z); };
    else
        g_queryChunkSnapshotLight = nullptr;

    if (m_destroyChunkLightSnapshot)
        g_destroyChunkLightSnapshot = [](void* s) { g_modLoader.destroyChunkLightSnapshot(s); };
    else
        g_destroyChunkLightSnapshot = nullptr;

    if (m_beginEmitterFeed)
        g_beginEmitterFeed = []() { g_modLoader.dispatchBeginEmitterFeed(); };
    else
        g_beginEmitterFeed = nullptr;

    if (m_notifyEmitter)
        g_notifyEmitter = [](int id, int x, int y, int z, int s) { g_modLoader.dispatchNotifyEmitter(id, x, y, z, s); };
    else
        g_notifyEmitter = nullptr;

    if (m_endEmitterFeed)
        g_endEmitterFeed = []() { g_modLoader.dispatchEndEmitterFeed(); };
    else
        g_endEmitterFeed = nullptr;

    if (!m_tileChangedCallbacks.empty())
        g_notifyTileChanged = [](int x, int y, int z) { g_modLoader.dispatchTileChanged(x, y, z); };
    else
        g_notifyTileChanged = nullptr;

    // -----------------------------------------------------------------------
    // 6. Publish event bus hook slots.
    // -----------------------------------------------------------------------
    if (!m_serverTickCallbacks.empty())
        g_serverTick = []() { g_modLoader.tickServer(); };
    else
        g_serverTick = nullptr;

    if (!m_levelLoadCallbacks.empty())
        g_onLevelLoad = [](int s) { g_modLoader.dispatchLevelLoad(s); };
    else
        g_onLevelLoad = nullptr;

    if (!m_levelUnloadCallbacks.empty())
        g_onLevelUnload = [](int s) { g_modLoader.dispatchLevelUnload(s); };
    else
        g_onLevelUnload = nullptr;

    if (!m_playerJoinCallbacks.empty())
        g_onPlayerJoin = [](int id, const char* n) { g_modLoader.dispatchPlayerJoin(id, n); };
    else
        g_onPlayerJoin = nullptr;

    if (!m_playerLeaveCallbacks.empty())
        g_onPlayerLeave = [](int id, const char* n) { g_modLoader.dispatchPlayerLeave(id, n); };
    else
        g_onPlayerLeave = nullptr;

    // -----------------------------------------------------------------------
    // 7. Publish content lifecycle hooks so the engine fires them at the
    //    right static-init points (Tile::staticCtor, Item::staticCtor,
    //    Recipes ctor, BiomeDecorator::decorate).
    // -----------------------------------------------------------------------
    if (!m_blocks.empty())
        g_registerModTiles = []() { g_modLoader.applyModTiles(); };
    else
        g_registerModTiles = nullptr;

    if (!m_items.empty())
        g_registerModItems = []() { g_modLoader.applyModItems(); };
    else
        g_registerModItems = nullptr;

    if (!m_recipes.empty())
        g_registerModRecipes = []() { g_modLoader.applyModRecipes(); };
    else
        g_registerModRecipes = nullptr;

    if (!m_tileEntities.empty())
        g_registerModTileEntities = []() { g_modLoader.applyModTileEntities(); };
    else
        g_registerModTileEntities = nullptr;

    if (!m_tileEntities.empty())
    {
        g_modTileEntityOnSave = [](const wchar_t* id, void* te, void* tag)
            { g_modLoader.dispatchTileEntitySave(id, te, tag); };
        g_modTileEntityOnLoad = [](const wchar_t* id, void* te, void* tag)
            { g_modLoader.dispatchTileEntityLoad(id, te, tag); };
    }
    else
    {
        g_modTileEntityOnSave = nullptr;
        g_modTileEntityOnLoad = nullptr;
    }

    if (!m_worldGens.empty())
        g_decorateChunk = [](void* lvl, int cx, int cz, unsigned int seed)
            { g_modLoader.decorateChunk(lvl, cx, cz, seed); };
    else
        g_decorateChunk = nullptr;

    if (!m_commands.empty())
        g_executeModCommandText = [](void* sender, const wchar_t* commandLine) -> bool
            { return g_modLoader.executeCommandText(sender, commandLine); };
    else
        g_executeModCommandText = nullptr;

    if (!m_guis.empty())
        g_openModGuiForBlock = [](int blockId,
                                  void* player,
                                  void* level,
                                  int x, int y, int z) -> bool
            { return g_modLoader.dispatchGuiOpenByBlock(blockId, player, level, x, y, z); };
    else
        g_openModGuiForBlock = nullptr;

    if (!m_blockCallbacks.empty())
    {
        g_modBlockOnUse = [](int blockId,
                             void* level, int x, int y, int z,
                             void* player,
                             int clickedFace,
                             float clickX, float clickY, float clickZ) -> bool
        {
            return g_modLoader.dispatchBlockOnUse(blockId, level, x, y, z, player,
                                                  clickedFace, clickX, clickY, clickZ);
        };
        g_modBlockOnBreak = [](int blockId,
                               void* level, int x, int y, int z,
                               void* player)
        {
            g_modLoader.dispatchBlockOnBreak(blockId, level, x, y, z, player);
        };
        g_modBlockOnPlaced = [](int blockId,
                                void* level, int x, int y, int z,
                                void* player, int itemAux)
        {
            g_modLoader.dispatchBlockOnPlaced(blockId, level, x, y, z, player, itemAux);
        };
        g_modBlockOnNeighborChanged = [](int blockId,
                                         void* level, int x, int y, int z,
                                         int neighborId)
        {
            g_modLoader.dispatchBlockOnNeighborChanged(blockId, level, x, y, z, neighborId);
        };
        g_modBlockOnTick = [](int blockId,
                              void* level, int x, int y, int z,
                              void* random)
        {
            g_modLoader.dispatchBlockOnTick(blockId, level, x, y, z, random);
        };
    }
    else
    {
        g_modBlockOnUse = nullptr;
        g_modBlockOnBreak = nullptr;
        g_modBlockOnPlaced = nullptr;
        g_modBlockOnNeighborChanged = nullptr;
        g_modBlockOnTick = nullptr;
    }

    if (!m_blockDrops.empty())
        g_modApplyBlockDrops = [](int blockId,
                                  void* level,
                                  int x, int y, int z,
                                  int playerBonusLevel) -> bool
            { return g_modLoader.applyBlockDrops(blockId, level, x, y, z, playerBonusLevel); };
    else
        g_modApplyBlockDrops = nullptr;

    if (!m_entityDrops.empty())
        g_modApplyEntityDrops = [](const wchar_t* entityId,
                                   void* entity,
                                   void* level,
                                   bool wasKilledByPlayer,
                                   int playerBonusLevel) -> bool
            { return g_modLoader.applyEntityDrops(entityId, entity, level, wasKilledByPlayer, playerBonusLevel); };
    else
        g_modApplyEntityDrops = nullptr;

    g_hostCollectModCreativeItems = [](ModCreativeItemHost* outItems, int maxItems) -> int
        { return g_modLoader.collectCreativeItems(outItems, maxItems); };
    g_hostCollectModCreativeCategories = [](ModCreativeCategoryHost* outCategories, int maxCategories) -> int
        { return g_modLoader.collectCreativeCategories(outCategories, maxCategories); };

    // -----------------------------------------------------------------------
    // 8. Print the visible load report.
    // -----------------------------------------------------------------------
    printLoadReport();
}

// ---------------------------------------------------------------------------
// unloadAllMods
// ---------------------------------------------------------------------------

void ModLoader::unloadAllMods()
{
    for (auto it = m_mods.rbegin(); it != m_mods.rend(); ++it)
    {
        if (it->shutdownFn)
            it->shutdownFn();

        FreeLibrary(static_cast<HMODULE>(it->handle));
        delete it->config;

        char msg[512];
        _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Unloaded %s", it->name.c_str());
        modLog("ModLoader", msg);
    }
    m_mods.clear();
    m_clientTickCallbacks.clear();
    m_serverTickCallbacks.clear();
    m_levelLoadCallbacks.clear();
    m_levelUnloadCallbacks.clear();
    m_playerJoinCallbacks.clear();
    m_playerLeaveCallbacks.clear();
    m_dynamicLightQueries.clear();
    m_prepareChunkLightSnapshot = nullptr;
    m_queryChunkSnapshotLight   = nullptr;
    m_destroyChunkLightSnapshot = nullptr;
    m_beginEmitterFeed          = nullptr;
    m_notifyEmitter             = nullptr;
    m_endEmitterFeed            = nullptr;
    m_tileChangedCallbacks.clear();
    g_queryDynamicLight         = nullptr;
    g_prepareChunkLightSnapshot = nullptr;
    g_queryChunkSnapshotLight   = nullptr;
    g_destroyChunkLightSnapshot = nullptr;
    g_beginEmitterFeed          = nullptr;
    g_notifyEmitter             = nullptr;
    g_endEmitterFeed            = nullptr;
    g_notifyTileChanged         = nullptr;
    g_serverTick                = nullptr;
    g_onLevelLoad               = nullptr;
    g_onLevelUnload             = nullptr;
    g_onPlayerJoin              = nullptr;
    g_onPlayerLeave             = nullptr;
    g_registerModTiles          = nullptr;
    g_registerModItems          = nullptr;
    g_registerModRecipes        = nullptr;
    g_registerModTileEntities   = nullptr;
    g_decorateChunk             = nullptr;
    g_executeModCommandText     = nullptr;
    g_openModGuiForBlock        = nullptr;
    g_modTileEntityOnSave       = nullptr;
    g_modTileEntityOnLoad       = nullptr;
    g_modBlockOnUse             = nullptr;
    g_modBlockOnBreak           = nullptr;
    g_modBlockOnPlaced          = nullptr;
    g_modBlockOnNeighborChanged = nullptr;
    g_modBlockOnTick            = nullptr;
    g_modApplyBlockDrops        = nullptr;
    g_modApplyEntityDrops       = nullptr;
    g_hostCollectModCreativeItems = nullptr;
    g_hostCollectModCreativeCategories = nullptr;
    m_entities.clear();
    m_assets.clear();
    m_recipes.clear();
    m_blockDrops.clear();
    m_entityDrops.clear();
    m_lootTables.clear();
    m_creativeCategories.clear();
    m_creativeItems.clear();
    m_worldGens.clear();
    m_blockCallbacks.clear();
    m_tileEntities.clear();
    m_guis.clear();
    m_commands.clear();
    m_structures.clear();
    m_structureTemplates.clear();
    m_serverPacketHandlers.clear();
    m_clientPacketHandlers.clear();
    m_projectiles.clear();
    m_activeProjectiles.clear();
    m_activeEntities.clear();
    m_blockRenderers.clear();
    m_itemRenderers.clear();
    m_entityRenderers.clear();
    m_projectileRenderers.clear();
    m_eventHandlers.clear();
    m_nextTileId = 256;
    m_nextItemId = 1000;
    m_nextCreativeCategoryId = 1000;
    m_nextRuntimeEntityId = 1;
    m_permissionsByMod.clear();
    m_activePermissionModId.clear();
}

// ---------------------------------------------------------------------------
// registerClientTick / tickClient
// ---------------------------------------------------------------------------

void ModLoader::registerClientTick(ClientTickFn fn)
{
    if (fn)
        m_clientTickCallbacks.push_back(fn);
}

void ModLoader::tickClient()
{
    struct WorldTickPayload { int isServer; } p{0};
    emitEvent("world.tick", nullptr, &p, sizeof(p));

    tickActiveEntities();
    tickActiveProjectiles(nullptr);
    for (ClientTickFn fn : m_clientTickCallbacks)
        fn();
}

// ---------------------------------------------------------------------------
// registerDynamicLightQuery / queryDynamicLight
// Multi-provider: accumulates all registered providers and returns the max.
// Output is clamped to [0, 15].
// ---------------------------------------------------------------------------

void ModLoader::registerDynamicLightQuery(QueryDynamicLightFn fn)
{
    if (fn)
        m_dynamicLightQueries.push_back(fn);
}

int ModLoader::queryDynamicLight(int x, int y, int z) const
{
    int best = 0;
    for (QueryDynamicLightFn fn : m_dynamicLightQueries)
    {
        int v = fn(x, y, z);
        if (v > best) best = v;
    }
    // Clamp to valid Minecraft light range 0-15
    if (best < 0)  best = 0;
    if (best > 15) best = 15;
    return best;
}

// ---------------------------------------------------------------------------
// Chunk bake snapshot API
// ---------------------------------------------------------------------------

void ModLoader::registerPrepareChunkLightSnapshot(PrepareChunkLightSnapshotFn fn)
{
    if (fn) m_prepareChunkLightSnapshot = fn;
}

void ModLoader::registerQueryChunkSnapshotLight(QueryChunkSnapshotLightFn fn)
{
    if (fn) m_queryChunkSnapshotLight = fn;
}

void ModLoader::registerDestroyChunkLightSnapshot(DestroyChunkLightSnapshotFn fn)
{
    if (fn) m_destroyChunkLightSnapshot = fn;
}

void* ModLoader::prepareChunkLightSnapshot(int chunkX, int chunkY, int chunkZ) const
{
    if (!m_prepareChunkLightSnapshot)
        return nullptr;
    return m_prepareChunkLightSnapshot(chunkX, chunkY, chunkZ);
}

int ModLoader::queryChunkSnapshotLight(void* snapshot, int x, int y, int z) const
{
    if (!m_queryChunkSnapshotLight || !snapshot)
        return 0;
    int v = m_queryChunkSnapshotLight(snapshot, x, y, z);
    if (v < 0)  v = 0;
    if (v > 15) v = 15;
    return v;
}

void ModLoader::destroyChunkLightSnapshot(void* snapshot) const
{
    if (m_destroyChunkLightSnapshot && snapshot)
        m_destroyChunkLightSnapshot(snapshot);
}

// ---------------------------------------------------------------------------
// Emitter feed
// ---------------------------------------------------------------------------

void ModLoader::registerBeginEmitterFeed(BeginEmitterFeedFn fn)
{
    if (fn) m_beginEmitterFeed = fn;
}

void ModLoader::registerNotifyEmitter(NotifyEmitterFn fn)
{
    if (fn) m_notifyEmitter = fn;
}

void ModLoader::registerEndEmitterFeed(EndEmitterFeedFn fn)
{
    if (fn) m_endEmitterFeed = fn;
}

void ModLoader::registerTileChanged(TileChangedFn fn)
{
    if (fn) m_tileChangedCallbacks.push_back(fn);
}

void ModLoader::dispatchTileChanged(int x, int y, int z) const
{
    for (auto fn : m_tileChangedCallbacks)
        fn(x, y, z);
}

// ---------------------------------------------------------------------------
// Server tick
// ---------------------------------------------------------------------------

void ModLoader::registerServerTick(ServerTickFn fn)
{
    if (fn) m_serverTickCallbacks.push_back(fn);
}

void ModLoader::tickServer()
{
    struct WorldTickPayload { int isServer; } p{1};
    emitEvent("world.tick", nullptr, &p, sizeof(p));

    tickActiveEntities();
    tickActiveProjectiles(nullptr);
    for (ServerTickFn fn : m_serverTickCallbacks)
        fn();
}

// ---------------------------------------------------------------------------
// Event bus
// ---------------------------------------------------------------------------

void ModLoader::registerLevelLoad(LevelLoadFn fn)
{
    if (fn) m_levelLoadCallbacks.push_back(fn);
}

void ModLoader::registerLevelUnload(LevelUnloadFn fn)
{
    if (fn) m_levelUnloadCallbacks.push_back(fn);
}

void ModLoader::registerPlayerJoin(PlayerJoinFn fn)
{
    if (fn) m_playerJoinCallbacks.push_back(fn);
}

void ModLoader::registerPlayerLeave(PlayerLeaveFn fn)
{
    if (fn) m_playerLeaveCallbacks.push_back(fn);
}

void ModLoader::dispatchLevelLoad(int isServer) const
{
    struct WorldLoadPayload { int isServer; } p{isServer};
    emitEvent("world.load", nullptr, &p, sizeof(p));

    for (LevelLoadFn fn : m_levelLoadCallbacks)
        fn(isServer);
}

void ModLoader::dispatchLevelUnload(int isServer) const
{
    for (LevelUnloadFn fn : m_levelUnloadCallbacks)
        fn(isServer);
}

void ModLoader::dispatchPlayerJoin(int entityId, const char* name) const
{
    struct PlayerJoinPayload { int entityId; const char* name; } p{entityId, name};
    emitEvent("player.join", nullptr, &p, sizeof(p));

    for (PlayerJoinFn fn : m_playerJoinCallbacks)
        fn(entityId, name);
}

void ModLoader::dispatchPlayerLeave(int entityId, const char* name) const
{
    for (PlayerLeaveFn fn : m_playerLeaveCallbacks)
        fn(entityId, name);
}

// ---------------------------------------------------------------------------
// Block / item / recipe / worldgen registration
// ---------------------------------------------------------------------------

int ModLoader::registerBlock(const ModBlockDef* def)
{
    if (!requirePermission("world.modify", "registerBlock"))
        return 0;

    if (!def || !def->id || !*def->id)
        return 0;
    if (m_nextTileId >= 4096)
    {
        modLog("ModLoader", "registerBlock: tile ID space exhausted (max 4095)");
        return 0;
    }

    BlockEntry e;
    e.id            = def->id;
    e.textureName   = def->textureName ? def->textureName : def->id;
    e.material      = def->material;
    e.hardness      = def->hardness;
    e.resistance    = def->resistance;
    e.lightEmission = def->lightEmission < 0 ? 0 : (def->lightEmission > 15 ? 15 : def->lightEmission);
    e.lightOpacity  = def->lightOpacity  < 0 ? 0 : (def->lightOpacity  > 15 ? 15 : def->lightOpacity);
    e.shape         = def->shape;
    e.flags         = def->flags;
    e.tileId        = m_nextTileId++;
    m_blocks.push_back(std::move(e));

    char msg[512];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE,
        "Queued block: %s -> tileId=%d", def->id, m_blocks.back().tileId);
    modLog("ModLoader", msg);
    return m_blocks.back().tileId;
}

int ModLoader::registerItem(const ModItemDef* def)
{
    if (!requirePermission("world.modify", "registerItem"))
        return 0;

    if (!def || !def->id || !*def->id)
        return 0;
    if (m_nextItemId >= 32000)
    {
        modLog("ModLoader", "registerItem: item ID space exhausted (max 31999)");
        return 0;
    }

    ItemEntry e;
    e.id          = def->id;
    e.textureName = def->textureName ? def->textureName : def->id;
    e.maxStack    = def->maxStack > 0 ? (def->maxStack > 64 ? 64 : def->maxStack) : 1;
    e.flags       = def->flags;
    e.itemId      = m_nextItemId++;
    m_items.push_back(std::move(e));

    char msg[512];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE,
        "Queued item: %s -> itemId=%d", def->id, m_items.back().itemId);
    modLog("ModLoader", msg);
    return m_items.back().itemId;
}

void ModLoader::registerRecipe(const ModRecipeDef* def)
{
    if (!requirePermission("world.modify", "registerRecipe"))
        return;

    if (!def) return;
    RecipeEntry e;
    e.outputId   = def->outputId;
    e.outputCount= def->outputCount;
    e.outputAux  = def->outputAux;
    e.recipeType = def->recipeType;
    e.width      = def->width  > 0 ? (def->width  > 3 ? 3 : def->width)  : 1;
    e.height     = def->height > 0 ? (def->height > 3 ? 3 : def->height) : 1;
    for (int i = 0; i < 9; ++i)
    {
        e.grid[i]    = def->grid[i];
        e.gridAux[i] = def->gridAux[i];
    }
    e.furnaceExp = def->furnaceExp;
    m_recipes.push_back(e);
    modLog("ModLoader", "Queued recipe");
}

void ModLoader::registerShapedRecipe(int outputId, int outputCount, int outputAux,
                                     int width, int height,
                                     const int* gridItems, const int* gridAux)
{
    if (!gridItems)
        return;

    ModRecipeDef r{};
    r.outputId = outputId;
    r.outputCount = outputCount;
    r.outputAux = outputAux;
    r.recipeType = MOD_RECIPE_SHAPED;
    r.width = width;
    r.height = height;

    int count = width * height;
    if (count < 0) count = 0;
    if (count > 9) count = 9;

    for (int i = 0; i < 9; ++i)
    {
        if (i < count)
        {
            r.grid[i] = gridItems[i];
            r.gridAux[i] = gridAux ? gridAux[i] : -1;
        }
        else
        {
            r.grid[i] = -1;
            r.gridAux[i] = -1;
        }
    }

    registerRecipe(&r);
}

void ModLoader::registerShapelessRecipe(int outputId, int outputCount, int outputAux,
                                        int count,
                                        const int* items, const int* auxVals)
{
    if (!items)
        return;

    if (count < 1) count = 1;
    if (count > 9) count = 9;

    ModRecipeDef r{};
    r.outputId = outputId;
    r.outputCount = outputCount;
    r.outputAux = outputAux;
    r.recipeType = MOD_RECIPE_SHAPELESS;
    r.width = count;
    r.height = 1;

    for (int i = 0; i < 9; ++i)
    {
        if (i < count)
        {
            r.grid[i] = items[i];
            r.gridAux[i] = auxVals ? auxVals[i] : -1;
        }
        else
        {
            r.grid[i] = -1;
            r.gridAux[i] = -1;
        }
    }

    registerRecipe(&r);
}

void ModLoader::registerSmeltingRecipe(int inputId, int inputAux,
                                       int outputId, int outputCount, int outputAux,
                                       float experience)
{
    ModRecipeDef r{};
    r.outputId = outputId;
    r.outputCount = outputCount;
    r.outputAux = outputAux;
    r.recipeType = MOD_RECIPE_FURNACE;
    r.width = 1;
    r.height = 1;
    r.grid[0] = inputId;
    r.gridAux[0] = inputAux;
    for (int i = 1; i < 9; ++i)
    {
        r.grid[i] = -1;
        r.gridAux[i] = -1;
    }
    r.furnaceExp = experience;
    registerRecipe(&r);
}

static DropEntry ConvertDropEntry(const ModDropEntry& src)
{
    DropEntry e;
    e.itemId = src.itemId;
    e.aux = src.aux;
    e.minCount = src.minCount;
    e.maxCount = src.maxCount;
    e.chance = src.chance;

    if (e.minCount < 0) e.minCount = 0;
    if (e.maxCount < e.minCount) e.maxCount = e.minCount;
    if (e.chance < 0.0f) e.chance = 0.0f;
    if (e.chance > 1.0f) e.chance = 1.0f;
    return e;
}

void ModLoader::registerBlockDrop(int blockId,
                                  const ModDropEntry* entries,
                                  int entryCount)
{
    if (!requirePermission("world.modify", "registerBlockDrop"))
        return;

    if (blockId <= 0 || !entries || entryCount <= 0)
        return;

    for (BlockDropEntry& b : m_blockDrops)
    {
        if (b.blockId == blockId)
        {
            b.drops.clear();
            b.drops.reserve(entryCount);
            for (int i = 0; i < entryCount; ++i)
                b.drops.push_back(ConvertDropEntry(entries[i]));
            modLog("ModLoader", "Updated block drop table");
            return;
        }
    }

    BlockDropEntry e;
    e.blockId = blockId;
    e.drops.reserve(entryCount);
    for (int i = 0; i < entryCount; ++i)
        e.drops.push_back(ConvertDropEntry(entries[i]));
    m_blockDrops.push_back(e);
    modLog("ModLoader", "Queued block drop table");
}

void ModLoader::registerEntityDrop(const char* entityId,
                                   const ModDropEntry* entries,
                                   int entryCount)
{
    if (!requirePermission("world.modify", "registerEntityDrop"))
        return;

    if (!entityId || !*entityId || !entries || entryCount <= 0)
        return;

    for (EntityDropEntry& d : m_entityDrops)
    {
        if (_stricmp(d.id.c_str(), entityId) == 0)
        {
            d.drops.clear();
            d.drops.reserve(entryCount);
            for (int i = 0; i < entryCount; ++i)
                d.drops.push_back(ConvertDropEntry(entries[i]));
            modLog("ModLoader", "Updated entity drop table");
            return;
        }
    }

    EntityDropEntry d;
    d.id = entityId;
    d.drops.reserve(entryCount);
    for (int i = 0; i < entryCount; ++i)
        d.drops.push_back(ConvertDropEntry(entries[i]));
    m_entityDrops.push_back(d);
    modLog("ModLoader", "Queued entity drop table");
}

void ModLoader::registerLootTable(const char* tableId,
                                  const ModDropEntry* entries,
                                  int entryCount)
{
    if (!requirePermission("world.modify", "registerLootTable"))
        return;

    if (!tableId || !*tableId || !entries || entryCount <= 0)
        return;

    for (LootTableEntry& t : m_lootTables)
    {
        if (_stricmp(t.id.c_str(), tableId) == 0)
        {
            t.drops.clear();
            t.drops.reserve(entryCount);
            for (int i = 0; i < entryCount; ++i)
                t.drops.push_back(ConvertDropEntry(entries[i]));
            modLog("ModLoader", "Updated loot table");
            return;
        }
    }

    LootTableEntry t;
    t.id = tableId;
    t.drops.reserve(entryCount);
    for (int i = 0; i < entryCount; ++i)
        t.drops.push_back(ConvertDropEntry(entries[i]));
    m_lootTables.push_back(t);
    modLog("ModLoader", "Queued loot table");
}

int ModLoader::registerCreativeCategory(const char* id,
                                        const char* displayName,
                                        int baseGroup)
{
    if (!id || !*id)
        return 0;

    for (const CreativeCategoryEntry& c : m_creativeCategories)
    {
        if (_stricmp(c.name.c_str(), id) == 0)
            return c.id;
    }

    CreativeCategoryEntry c;
    c.id = m_nextCreativeCategoryId++;
    c.name = id;
    c.displayName = displayName ? displayName : id;
    c.baseGroup = baseGroup;
    if (c.baseGroup < 0)
        c.baseGroup = MOD_CREATIVE_GROUP_MISC;
    m_creativeCategories.push_back(c);
    modLog("ModLoader", "Queued creative category");
    return c.id;
}

void ModLoader::registerCreativeItem(const ModCreativeItem* def)
{
    if (!def || def->itemId <= 0)
        return;

    CreativeItemEntry e;
    e.itemId = def->itemId;
    e.aux = def->aux;
    e.categoryId = def->categoryId;
    e.displayName = def->displayName ? def->displayName : "";
    m_creativeItems.push_back(e);
}

int ModLoader::collectCreativeItems(ModCreativeItemHost* outItems, int maxItems) const
{
    int total = static_cast<int>(m_creativeItems.size());
    if (!outItems || maxItems <= 0)
        return total;

    int count = (total < maxItems) ? total : maxItems;
    for (int i = 0; i < count; ++i)
    {
        const CreativeItemEntry& src = m_creativeItems[i];

        int resolvedCategory = src.categoryId;
        for (const CreativeCategoryEntry& c : m_creativeCategories)
        {
            if (c.id == src.categoryId)
            {
                resolvedCategory = c.baseGroup;
                break;
            }
        }

        outItems[i].itemId = src.itemId;
        outItems[i].aux = src.aux;
        outItems[i].categoryId = resolvedCategory;
        outItems[i].displayName = src.displayName.empty() ? nullptr : src.displayName.c_str();
    }
    return total;
}

int ModLoader::collectCreativeCategories(ModCreativeCategoryHost* outCategories, int maxCategories) const
{
    int total = static_cast<int>(m_creativeCategories.size());
    if (!outCategories || maxCategories <= 0)
        return total;

    int count = (total < maxCategories) ? total : maxCategories;
    for (int i = 0; i < count; ++i)
    {
        const CreativeCategoryEntry& src = m_creativeCategories[i];
        outCategories[i].categoryId = src.id;
        outCategories[i].baseGroup = src.baseGroup;
        outCategories[i].displayName = src.displayName.empty() ? nullptr : src.displayName.c_str();
    }
    return total;
}

bool ModLoader::loadModJson(const char* relativePath)
{
    if (!requirePermission("assets", "loadModJson"))
        return false;

    if (!relativePath || !*relativePath || m_currentModDir.empty())
        return false;

    std::string rel = relativePath;
    std::replace(rel.begin(), rel.end(), '/', '\\');
    std::string full = m_currentModDir + "\\" + rel;
    return loadModJsonFile(full);
}

bool ModLoader::loadModAssets(const char* manifestRelativePath)
{
    if (!requirePermission("assets", "loadModAssets"))
        return false;

    if (!manifestRelativePath || !*manifestRelativePath || m_currentModDir.empty())
        return false;

    std::string rel = manifestRelativePath;
    std::replace(rel.begin(), rel.end(), '/', '\\');
    std::string full = m_currentModDir + "\\" + rel;
    return loadModAssetsFile(full);
}

void ModLoader::autoScanModContentJson(const std::string& modDir)
{
    if (modDir.empty())
        return;

    char pattern[MAX_PATH];
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\content\\*.json", modDir.c_str());

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        std::string full = modDir + "\\content\\" + fd.cFileName;
        loadModJsonFile(full);
    } while (FindNextFileA(h, &fd));

    FindClose(h);
}

bool ModLoader::loadModJsonFile(const std::string& absolutePath)
{
    std::string norm = toLowerAscii(absolutePath);
    for (const std::string& s : m_loadedJsonPathsForCurrentMod)
    {
        if (toLowerAscii(s) == norm)
            return true;
    }

    std::string src;
    if (!readTextFile(absolutePath, src))
        return false;

    std::string lower = toLowerAscii(absolutePath);
    std::vector<std::string> objs;
    if (!jsonSplitTopLevelArrayObjects(src, objs))
    {
        m_loadedJsonPathsForCurrentMod.push_back(absolutePath);
        return true;
    }

    if (lower.find("blocks.json") != std::string::npos)
    {
        for (const std::string& o : objs)
        {
            std::string id = jsonReadField(o, "id");
            if (id.empty())
                continue;

            ModBlockDef b{};
            b.id = id.c_str();
            std::string tex = jsonReadField(o, "textureName");
            b.textureName = tex.empty() ? b.id : tex.c_str();
            if (!jsonReadIntField(o, "material", &b.material)) b.material = MOD_MATERIAL_ROCK;
            if (!jsonReadFloatField(o, "hardness", &b.hardness)) b.hardness = 1.0f;
            if (!jsonReadFloatField(o, "resistance", &b.resistance)) b.resistance = 6.0f;
            if (!jsonReadIntField(o, "lightEmission", &b.lightEmission)) b.lightEmission = 0;
            if (!jsonReadIntField(o, "lightOpacity", &b.lightOpacity)) b.lightOpacity = 15;
            if (!jsonReadIntField(o, "shape", &b.shape)) b.shape = 0;
            if (!jsonReadIntField(o, "flags", &b.flags)) b.flags = 0;

            int newId = registerBlock(&b);

            int creativeCategoryId = MOD_CREATIVE_GROUP_MISC;
            jsonReadIntField(o, "creativeCategoryId", &creativeCategoryId);
            if (newId > 0)
            {
                ModCreativeItem ci{};
                ci.itemId = newId;
                ci.aux = 0;
                ci.categoryId = creativeCategoryId;
                std::string dn = jsonReadField(o, "displayName");
                ci.displayName = dn.empty() ? nullptr : dn.c_str();
                registerCreativeItem(&ci);
            }
        }
    }
    else if (lower.find("items.json") != std::string::npos)
    {
        for (const std::string& o : objs)
        {
            std::string id = jsonReadField(o, "id");
            if (id.empty())
                continue;

            ModItemDef it{};
            it.id = id.c_str();
            std::string tex = jsonReadField(o, "textureName");
            it.textureName = tex.empty() ? it.id : tex.c_str();
            if (!jsonReadIntField(o, "maxStack", &it.maxStack)) it.maxStack = 64;
            if (!jsonReadIntField(o, "flags", &it.flags)) it.flags = 0;

            int newId = registerItem(&it);

            int creativeCategoryId = MOD_CREATIVE_GROUP_MISC;
            jsonReadIntField(o, "creativeCategoryId", &creativeCategoryId);
            if (newId > 0)
            {
                ModCreativeItem ci{};
                ci.itemId = newId;
                ci.aux = 0;
                ci.categoryId = creativeCategoryId;
                std::string dn = jsonReadField(o, "displayName");
                ci.displayName = dn.empty() ? nullptr : dn.c_str();
                registerCreativeItem(&ci);
            }
        }
    }
    else if (lower.find("recipes.json") != std::string::npos)
    {
        for (const std::string& o : objs)
        {
            std::string type = toLowerAscii(jsonReadField(o, "type"));
            int outputId = 0, outputCount = 1, outputAux = 0;
            jsonReadIntField(o, "outputId", &outputId);
            jsonReadIntField(o, "outputCount", &outputCount);
            jsonReadIntField(o, "outputAux", &outputAux);

            if (type == "shaped")
            {
                int width = 1, height = 1;
                jsonReadIntField(o, "width", &width);
                jsonReadIntField(o, "height", &height);
                std::vector<int> grid;
                std::vector<int> aux;
                if (!jsonReadIntArrayField(o, "grid", grid))
                    continue;
                jsonReadIntArrayField(o, "gridAux", aux);

                int cellCount = width * height;
                if (cellCount < 1) cellCount = 1;
                if (cellCount > 9) cellCount = 9;
                std::vector<int> shapedGrid(cellCount, -1);
                std::vector<int> shapedAux(cellCount, -1);
                for (int i = 0; i < cellCount && i < static_cast<int>(grid.size()); ++i)
                    shapedGrid[i] = grid[i];
                for (int i = 0; i < cellCount && i < static_cast<int>(aux.size()); ++i)
                    shapedAux[i] = aux[i];

                registerShapedRecipe(outputId, outputCount, outputAux,
                                     width, height,
                                     shapedGrid.data(), shapedAux.data());
            }
            else if (type == "shapeless")
            {
                std::vector<int> items;
                std::vector<int> aux;
                if (!jsonReadIntArrayField(o, "items", items))
                    continue;
                if (items.empty())
                    continue;
                jsonReadIntArrayField(o, "aux", aux);
                registerShapelessRecipe(outputId, outputCount, outputAux,
                                        static_cast<int>(items.size()),
                                        items.data(), aux.empty() ? nullptr : aux.data());
            }
            else if (type == "smelting")
            {
                int inputId = 0, inputAux = -1;
                float exp = 0.0f;
                if (!jsonReadIntField(o, "inputId", &inputId))
                    continue;
                jsonReadIntField(o, "inputAux", &inputAux);
                jsonReadFloatField(o, "experience", &exp);
                registerSmeltingRecipe(inputId, inputAux, outputId, outputCount, outputAux, exp);
            }
        }
    }
    else if (lower.find("loot_tables.json") != std::string::npos)
    {
        for (const std::string& o : objs)
        {
            std::string kind = toLowerAscii(jsonReadField(o, "kind"));
            std::string dropsRaw;
            if (!jsonReadRawValue(o, "drops", dropsRaw))
                continue;

            std::vector<ModDropEntry> drops;
            if (!parseDropEntriesJson(dropsRaw, drops))
                continue;

            if (kind == "block")
            {
                int blockId = 0;
                if (!jsonReadIntField(o, "blockId", &blockId))
                    continue;
                registerBlockDrop(blockId, drops.data(), static_cast<int>(drops.size()));
            }
            else if (kind == "entity")
            {
                std::string entityId = jsonReadField(o, "entityId");
                if (entityId.empty())
                    continue;
                registerEntityDrop(entityId.c_str(), drops.data(), static_cast<int>(drops.size()));
            }
            else if (kind == "table")
            {
                std::string tableId = jsonReadField(o, "tableId");
                if (tableId.empty())
                    continue;
                registerLootTable(tableId.c_str(), drops.data(), static_cast<int>(drops.size()));
            }
        }
    }

    m_loadedJsonPathsForCurrentMod.push_back(absolutePath);
    return true;
}

bool ModLoader::loadModAssetsFile(const std::string& absolutePath)
{
    std::string src;
    if (!readTextFile(absolutePath, src))
        return false;

    std::string assetsDir = absolutePath;
    std::size_t slash = assetsDir.find_last_of("\\/");
    assetsDir = (slash == std::string::npos) ? m_currentModDir : assetsDir.substr(0, slash);

    auto loadKey = [&](const char* key, std::vector<std::string>& out) -> bool
    {
        std::string raw;
        if (!jsonReadRawValue(src, key, raw))
        {
            out.clear();
            return true;
        }
        return parseStringArrayJson(raw, out);
    };

    std::vector<std::string> langs;
    std::vector<std::string> textures;
    std::vector<std::string> sounds;
    std::vector<std::string> models;
    if (!loadKey("lang", langs)) return false;
    if (!loadKey("textures", textures)) return false;
    if (!loadKey("sounds", sounds)) return false;
    if (!loadKey("models", models)) return false;

    auto existsFile = [&](const std::string& relPath) -> bool
    {
        std::string p = relPath;
        std::replace(p.begin(), p.end(), '/', '\\');
        std::string full = assetsDir + "\\" + p;
        DWORD attr = GetFileAttributesA(full.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    };

    auto registerPaths = [&](const std::vector<std::string>& paths, auto&& registerFn)
    {
        for (const std::string& p : paths)
        {
            if (!existsFile(p))
                continue;
            registerFn(p.c_str(), p.c_str());
        }
    };

    // Strict priority: lang, then textures, then sounds, then models.
    registerPaths(langs,    [&](const char* id, const char* path) { registerLang(id, path); });
    registerPaths(textures, [&](const char* id, const char* path) { registerTexture(id, path); });
    registerPaths(sounds,   [&](const char* id, const char* path) { registerSound(id, path); });
    registerPaths(models,   [&](const char* id, const char* path) { registerModel(id, path); });

    return true;
}

bool ModLoader::applyBlockDrops(int blockId,
                                void* levelHandle,
                                int x, int y, int z,
                                int playerBonusLevel) const
{
    if (!levelHandle || !g_hostApplyBlockDropEntries)
        return false;

    for (const BlockDropEntry& b : m_blockDrops)
    {
        if (b.blockId != blockId)
            continue;

        std::vector<ModDropEntryHost> hostEntries;
        hostEntries.reserve(b.drops.size());
        for (const DropEntry& d : b.drops)
        {
            ModDropEntryHost h;
            h.itemId = d.itemId;
            h.aux = d.aux;
            h.minCount = d.minCount;
            h.maxCount = d.maxCount;
            h.chance = d.chance;
            hostEntries.push_back(h);
        }

        return g_hostApplyBlockDropEntries(levelHandle, x, y, z,
                                           hostEntries.data(), static_cast<int>(hostEntries.size()),
                                           playerBonusLevel);
    }

    return false;
}

bool ModLoader::applyEntityDrops(const wchar_t* entityId,
                                 void* entityHandle,
                                 void* levelHandle,
                                 bool wasKilledByPlayer,
                                 int playerBonusLevel) const
{
    if (!entityId || !entityHandle || !g_hostApplyEntityDropEntries)
        return false;

    std::string id = narrowAscii(std::wstring(entityId));

    for (const EntityDropEntry& d : m_entityDrops)
    {
        if (_stricmp(d.id.c_str(), id.c_str()) != 0)
            continue;

        std::vector<ModDropEntryHost> hostEntries;
        hostEntries.reserve(d.drops.size());
        for (const DropEntry& e : d.drops)
        {
            ModDropEntryHost h;
            h.itemId = e.itemId;
            h.aux = e.aux;
            h.minCount = e.minCount;
            h.maxCount = e.maxCount;
            h.chance = e.chance;
            hostEntries.push_back(h);
        }

        return g_hostApplyEntityDropEntries(entityHandle, levelHandle,
                                            hostEntries.data(), static_cast<int>(hostEntries.size()),
                                            wasKilledByPlayer, playerBonusLevel);
    }

    return false;
}

void ModLoader::registerWorldGen(const ModWorldGenDef* def)
{
    if (!requirePermission("world.modify", "registerWorldGen"))
        return;

    if (!def || !def->generate) return;
    WorldGenEntry e;
    e.generate = def->generate;
    e.weight   = def->weight > 0 ? def->weight : 1;
    m_worldGens.push_back(e);
    modLog("ModLoader", "Queued worldgen");
}

void ModLoader::registerBlockCallbacks(int blockId, const ModBlockCallbacks* callbacks)
{
    if (!requirePermission("world.modify", "registerBlockCallbacks"))
        return;

    if (!callbacks || blockId <= 0)
        return;

    for (BlockCallbackEntry& e : m_blockCallbacks)
    {
        if (e.blockId == blockId)
        {
            e.callbacks = *callbacks;
            modLog("ModLoader", "Updated block callbacks");
            return;
        }
    }

    BlockCallbackEntry entry;
    entry.blockId = blockId;
    entry.callbacks = *callbacks;
    m_blockCallbacks.push_back(entry);
    modLog("ModLoader", "Queued block callbacks");
}

void ModLoader::registerTileEntity(const ModTileEntityDef* def)
{
    if (!requirePermission("world.modify", "registerTileEntity"))
        return;

    if (!def || !def->id || !*def->id)
        return;
    TileEntityEntry e;
    e.id = def->id;
    e.blockId = def->blockId;
    e.create = def->create;
    e.onSave = def->onSave;
    e.onLoad = def->onLoad;
    m_tileEntities.push_back(e);
    modLog("ModLoader", "Queued tile entity");
}

void ModLoader::registerGui(const ModGuiDef* def)
{
    if (!requirePermission("world.modify", "registerGui"))
        return;

    if (!def || !def->id || !*def->id)
        return;
    GuiEntry e;
    e.id = def->id;
    e.blockId = def->blockId;
    e.open = def->open;
    m_guis.push_back(e);
    modLog("ModLoader", "Queued GUI");
}

void ModLoader::registerCommand(const ModCommandDef* def)
{
    if (!requirePermission("commands", "registerCommand"))
        return;

    if (!def || !def->name || !*def->name)
        return;
    CommandEntry e;
    e.name = def->name;
    e.help = def->help ? def->help : "";
    e.execute = def->execute;
    m_commands.push_back(e);
    modLog("ModLoader", "Queued command");
}

void ModLoader::registerStructure(const ModStructureDef* def)
{
    if (!requirePermission("world.modify", "registerStructure"))
        return;

    if (!def || !def->id || !*def->id || !def->place)
        return;
    StructureEntry e;
    e.id = def->id;
    e.weight = def->weight > 0 ? def->weight : 1;
    e.place = def->place;
    m_structures.push_back(e);
    modLog("ModLoader", "Queued structure");
}

void ModLoader::registerStructureTemplate(const ModStructureTemplateDef* def)
{
    if (!requirePermission("world.modify", "registerStructureTemplate"))
        return;

    if (!def || !def->id || !*def->id ||
        !def->palette || def->paletteCount <= 0 ||
        !def->blocks || def->blockCount <= 0)
        return;

    for (StructureTemplateEntry& existing : m_structureTemplates)
    {
        if (_stricmp(existing.id.c_str(), def->id) == 0)
        {
            existing.palette.clear();
            existing.blocks.clear();

            existing.palette.reserve(def->paletteCount);
            for (int i = 0; i < def->paletteCount; ++i)
            {
                StructurePaletteEntry p;
                p.tileId = def->palette[i].tileId;
                p.data   = def->palette[i].data;
                existing.palette.push_back(p);
            }

            existing.blocks.reserve(def->blockCount);
            for (int i = 0; i < def->blockCount; ++i)
            {
                const ModStructureBlockEntry& src = def->blocks[i];
                if (src.paletteIndex < 0 || src.paletteIndex >= def->paletteCount)
                    continue;

                StructureBlockEntry b;
                b.x = src.x;
                b.y = src.y;
                b.z = src.z;
                b.paletteIndex = src.paletteIndex;
                existing.blocks.push_back(b);
            }

            modLog("ModLoader", "Updated structure template");
            return;
        }
    }

    StructureTemplateEntry e;
    e.id = def->id;
    e.palette.reserve(def->paletteCount);
    for (int i = 0; i < def->paletteCount; ++i)
    {
        StructurePaletteEntry p;
        p.tileId = def->palette[i].tileId;
        p.data   = def->palette[i].data;
        e.palette.push_back(p);
    }

    e.blocks.reserve(def->blockCount);
    for (int i = 0; i < def->blockCount; ++i)
    {
        const ModStructureBlockEntry& src = def->blocks[i];
        if (src.paletteIndex < 0 || src.paletteIndex >= def->paletteCount)
            continue;

        StructureBlockEntry b;
        b.x = src.x;
        b.y = src.y;
        b.z = src.z;
        b.paletteIndex = src.paletteIndex;
        e.blocks.push_back(b);
    }

    m_structureTemplates.push_back(e);
    modLog("ModLoader", "Queued structure template");
}

static int NormalizeStructureRotation(int rotation)
{
    if (rotation >= 0 && rotation <= 3)
        return rotation;

    if ((rotation % 90) == 0)
    {
        int quarterTurns = rotation / 90;
        quarterTurns %= 4;
        if (quarterTurns < 0)
            quarterTurns += 4;
        return quarterTurns;
    }

    return 0;
}

bool ModLoader::placeStructure(void* level,
                               const char* templateId,
                               int originX, int originY, int originZ,
                               int rotation)
{
    if (!level || !templateId || !*templateId || !g_worldGenPlaceBlock)
        return false;

    const StructureTemplateEntry* tmpl = nullptr;
    for (const StructureTemplateEntry& e : m_structureTemplates)
    {
        if (_stricmp(e.id.c_str(), templateId) == 0)
        {
            tmpl = &e;
            break;
        }
    }

    if (!tmpl || tmpl->palette.empty() || tmpl->blocks.empty())
        return false;

    int rot = NormalizeStructureRotation(rotation);
    int placed = 0;
    for (const StructureBlockEntry& b : tmpl->blocks)
    {
        if (b.paletteIndex < 0 || b.paletteIndex >= static_cast<int>(tmpl->palette.size()))
            continue;

        int rx = b.x;
        int rz = b.z;
        switch (rot)
        {
        case MOD_STRUCTURE_ROT_90:  rx = -b.z; rz =  b.x; break;
        case MOD_STRUCTURE_ROT_180: rx = -b.x; rz = -b.z; break;
        case MOD_STRUCTURE_ROT_270: rx =  b.z; rz = -b.x; break;
        default: break;
        }

        const StructurePaletteEntry& p = tmpl->palette[b.paletteIndex];
        g_worldGenPlaceBlock(level,
                             originX + rx,
                             originY + b.y,
                             originZ + rz,
                             p.tileId,
                             p.data);
        ++placed;
    }

    return placed > 0;
}

void ModLoader::registerServerPacketHandler(ModServerPacketHandlerFn fn)
{
    if (!requirePermission("network.server", "registerServerPacketHandler"))
        return;

    if (!fn)
        return;
    ServerPacketHandlerEntry e;
    e.fn = fn;
    m_serverPacketHandlers.push_back(e);
}

void ModLoader::registerClientPacketHandler(ModClientPacketHandlerFn fn)
{
    if (!requirePermission("network.client", "registerClientPacketHandler"))
        return;

    if (!fn)
        return;
    ClientPacketHandlerEntry e;
    e.fn = fn;
    m_clientPacketHandlers.push_back(e);
}

bool ModLoader::dispatchServerPacket(void* sender,
                                     const char* channel,
                                     const void* payload,
                                     int size) const
{
    if (!channel || !*channel)
        return false;

    bool handled = false;
    for (const ServerPacketHandlerEntry& e : m_serverPacketHandlers)
    {
        if (!e.fn)
            continue;
        if (e.fn(sender, channel, payload, size))
            handled = true;
    }
    return handled;
}

bool ModLoader::dispatchClientPacket(const char* channel,
                                     const void* payload,
                                     int size) const
{
    if (!channel || !*channel)
        return false;

    bool handled = false;
    for (const ClientPacketHandlerEntry& e : m_clientPacketHandlers)
    {
        if (!e.fn)
            continue;
        if (e.fn(channel, payload, size))
            handled = true;
    }
    return handled;
}

bool ModLoader::sendToServer(const char* channel, const void* payload, int size) const
{
    if (!requirePermission("network.client", "sendToServer"))
        return false;

    if (!g_hostSendModPacketToServer || !channel || !*channel || size < 0)
        return false;
    std::wstring wchannel = widenAscii(channel);
    return g_hostSendModPacketToServer(wchannel.c_str(), payload, size);
}

bool ModLoader::sendToClient(void* player, const char* channel, const void* payload, int size) const
{
    if (!requirePermission("network.server", "sendToClient"))
        return false;

    if (!g_hostSendModPacketToClient || !player || !channel || !*channel || size < 0)
        return false;
    std::wstring wchannel = widenAscii(channel);
    return g_hostSendModPacketToClient(player, wchannel.c_str(), payload, size);
}

int ModLoader::broadcastToTracking(void* level,
                                   int x, int y, int z, int radius,
                                   const char* channel,
                                   const void* payload,
                                   int size) const
{
    if (!requirePermission("network.server", "broadcastToTracking"))
        return 0;

    if (!g_hostBroadcastModPacketToTracking || !level || !channel || !*channel || size < 0)
        return 0;
    std::wstring wchannel = widenAscii(channel);
    return g_hostBroadcastModPacketToTracking(level, x, y, z, radius,
                                              wchannel.c_str(), payload, size);
}

std::string ModLoader::buildRegistryHandshakeBlob() const
{
    std::vector<std::string> entries;
    entries.reserve(m_mods.size());
    for (const LoadedMod& m : m_mods)
    {
        std::string id = m.modId.empty() ? m.name : m.modId;
        std::string version = m.version.empty() ? "0" : m.version;
        entries.push_back(id + "=" + version);
    }
    std::sort(entries.begin(), entries.end());

    std::string blob;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i != 0) blob += ';';
        blob += entries[i];
    }
    return blob;
}

bool ModLoader::validateRegistryHandshakeBlob(const char* remoteBlob, std::string* outReason) const
{
    std::string local = buildRegistryHandshakeBlob();
    std::string remote = remoteBlob ? remoteBlob : "";
    if (local == remote)
        return true;

    if (outReason)
        *outReason = "mod registry mismatch";
    return false;
}

void ModLoader::registerProjectile(const ModProjectileDef* def)
{
    if (!requirePermission("entities", "registerProjectile"))
        return;

    if (!def || !def->id || !*def->id || !def->spawn)
        return;
    ProjectileEntry e;
    e.id = def->id;
    e.spawn = def->spawn;
    e.update = def->update;
    m_projectiles.push_back(e);
    modLog("ModLoader", "Queued projectile");
}

void* ModLoader::spawnProjectile(const char* id,
                                 void* level,
                                 void* owner,
                                 float x, float y, float z,
                                 float vx, float vy, float vz)
{
    if (!requirePermission("entities", "spawnProjectile"))
        return nullptr;

    if (!id || !*id)
        return nullptr;

    for (const ProjectileEntry& p : m_projectiles)
    {
        if (_stricmp(p.id.c_str(), id) == 0 && p.spawn)
        {
            void* handle = p.spawn(level, owner, x, y, z, vx, vy, vz);
            if (handle)
            {
                ActiveProjectileEntry ap;
                ap.id = p.id;
                ap.handle = handle;
                ap.update = p.update;
                m_activeProjectiles.push_back(ap);
                modLog("ModLoader", "Spawned projectile");
            }
            return handle;
        }
    }
    return nullptr;
}

void ModLoader::tickActiveProjectiles(void* level)
{
    if (m_activeProjectiles.empty())
        return;

    std::vector<ActiveProjectileEntry> next;
    next.reserve(m_activeProjectiles.size());

    for (const ActiveProjectileEntry& p : m_activeProjectiles)
    {
        if (!p.handle)
            continue;
        if (!p.update)
        {
            next.push_back(p);
            continue;
        }

        bool keep = p.update(p.handle, level);
        if (keep)
            next.push_back(p);
    }

    m_activeProjectiles.swap(next);
}

bool ModLoader::dispatchBlockOnUse(int blockId,
                                   void* level, int x, int y, int z,
                                   void* player,
                                   int clickedFace,
                                   float clickX, float clickY, float clickZ) const
{
    struct PlayerUseItemPayload
    {
        int blockId;
        int x, y, z;
        int clickedFace;
        float clickX, clickY, clickZ;
    } payload{ blockId, x, y, z, clickedFace, clickX, clickY, clickZ };
    emitEvent("player.use_item", player, &payload, sizeof(payload));

    for (const BlockCallbackEntry& e : m_blockCallbacks)
    {
        if (e.blockId == blockId && e.callbacks.onUse)
            return e.callbacks.onUse(level, x, y, z, player,
                                     clickedFace, clickX, clickY, clickZ);
    }
    return false;
}

void ModLoader::dispatchBlockOnBreak(int blockId,
                                     void* level, int x, int y, int z,
                                     void* player) const
{
    struct BlockBreakPayload { int blockId; int x, y, z; } payload{ blockId, x, y, z };
    emitEvent("block.break", player, &payload, sizeof(payload));

    for (const BlockCallbackEntry& e : m_blockCallbacks)
    {
        if (e.blockId == blockId && e.callbacks.onBreak)
            e.callbacks.onBreak(level, x, y, z, player);
    }
}

void ModLoader::dispatchBlockOnPlaced(int blockId,
                                      void* level, int x, int y, int z,
                                      void* player, int itemAux) const
{
    struct BlockPlacePayload { int blockId; int x, y, z; int itemAux; } payload{ blockId, x, y, z, itemAux };
    emitEvent("block.place", player, &payload, sizeof(payload));

    for (const BlockCallbackEntry& e : m_blockCallbacks)
    {
        if (e.blockId == blockId && e.callbacks.onPlaced)
            e.callbacks.onPlaced(level, x, y, z, player, itemAux);
    }
}

void ModLoader::dispatchBlockOnNeighborChanged(int blockId,
                                               void* level, int x, int y, int z,
                                               int neighborId) const
{
    for (const BlockCallbackEntry& e : m_blockCallbacks)
    {
        if (e.blockId == blockId && e.callbacks.onNeighborChanged)
            e.callbacks.onNeighborChanged(level, x, y, z, neighborId);
    }
}

void ModLoader::dispatchBlockOnTick(int blockId,
                                    void* level, int x, int y, int z,
                                    void* random) const
{
    for (const BlockCallbackEntry& e : m_blockCallbacks)
    {
        if (e.blockId == blockId && e.callbacks.onTick)
            e.callbacks.onTick(level, x, y, z, random);
    }
}

// ---------------------------------------------------------------------------
// applyModTiles -- called by g_registerModTiles from Tile::staticCtor()
// ---------------------------------------------------------------------------

void ModLoader::applyModTiles()
{
    for (const BlockEntry& e : m_blocks)
    {
        if (!g_hostCreateModTile) break;

        // Convert narrow texture name to wide for the engine API.
        std::wstring wTex(e.textureName.begin(), e.textureName.end());

        int result = g_hostCreateModTile(
            e.tileId, wTex.c_str(),
            e.material, e.hardness, e.resistance,
            e.lightEmission, e.lightOpacity, e.shape, e.flags);

        char msg[512];
        if (result)
            _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "Applied block: %s (tileId=%d)", e.id.c_str(), result);
        else
            _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "FAILED to apply block: %s (tileId=%d)", e.id.c_str(), e.tileId);
        modLog("ModLoader", msg);
    }
}

// ---------------------------------------------------------------------------
// applyModItems -- called by g_registerModItems from Item::staticCtor()
// ---------------------------------------------------------------------------

void ModLoader::applyModItems()
{
    for (const ItemEntry& e : m_items)
    {
        if (!g_hostCreateModItem) break;

        std::wstring wTex(e.textureName.begin(), e.textureName.end());

        int result = g_hostCreateModItem(
            e.itemId, wTex.c_str(), e.maxStack, e.flags);

        char msg[512];
        if (result)
            _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "Applied item: %s (itemId=%d)", e.id.c_str(), result);
        else
            _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "FAILED to apply item: %s (itemId=%d)", e.id.c_str(), e.itemId);
        modLog("ModLoader", msg);
    }
}

// ---------------------------------------------------------------------------
// applyModRecipes -- called by g_registerModRecipes from Recipes::Recipes()
// ---------------------------------------------------------------------------

void ModLoader::applyModRecipes()
{
    for (const RecipeEntry& e : m_recipes)
    {
        if (e.recipeType == 2 /*MOD_RECIPE_FURNACE*/)
        {
            if (g_hostAddFurnaceRecipe)
                g_hostAddFurnaceRecipe(
                    e.grid[0], e.outputId, e.outputCount,
                    e.outputAux, e.furnaceExp);
        }
        else if (e.recipeType == 1 /*MOD_RECIPE_SHAPELESS*/)
        {
            int count = e.width; // shapeless: ingredients in grid[0..width-1]
            if (g_hostAddShapelessRecipe)
                g_hostAddShapelessRecipe(
                    e.outputId, e.outputCount, e.outputAux,
                    count, e.grid, e.gridAux);
        }
        else // MOD_RECIPE_SHAPED
        {
            if (g_hostAddShapedRecipe)
                g_hostAddShapedRecipe(
                    e.outputId, e.outputCount, e.outputAux,
                    e.width, e.height, e.grid, e.gridAux);
        }
        modLog("ModLoader", "Applied recipe");
    }
}

// ---------------------------------------------------------------------------
// decorateChunk -- called by g_decorateChunk from BiomeDecorator::decorate()
// ---------------------------------------------------------------------------

void ModLoader::decorateChunk(void* levelHandle,
                               int chunkX, int chunkZ,
                               unsigned int seed)
{
    for (const WorldGenEntry& e : m_worldGens)
    {
        for (int w = 0; w < e.weight; ++w)
            e.generate(levelHandle, chunkX, chunkZ, seed, g_worldGenPlaceBlock);
    }

    for (const StructureEntry& s : m_structures)
    {
        for (int w = 0; w < s.weight; ++w)
            s.place(levelHandle, chunkX, chunkZ, seed, g_worldGenPlaceBlock);
    }
}

void ModLoader::registerEntity(const ModEntityDef* def)
{
    if (!requirePermission("entities", "registerEntity"))
        return;

    if (!def || !def->id || !*def->id || !def->create)
        return;

    for (EntityEntry& e : m_entities)
    {
        if (_stricmp(e.id.c_str(), def->id) == 0)
        {
            e.networkTypeId  = def->networkTypeId;
            e.primaryColor   = def->eggPrimaryColor;
            e.secondaryColor = def->eggSecondaryColor;
            e.create         = def->create;
            e.tick           = def->tick;
            e.onHurt         = def->onHurt;
            e.onDeath        = def->onDeath;
            e.onSave         = def->onSave;
            e.onLoad         = def->onLoad;
            modLog("ModLoader", "Updated custom entity");
            return;
        }
    }

    EntityEntry e;
    e.id             = def->id;
    e.networkTypeId  = def->networkTypeId;
    e.primaryColor   = def->eggPrimaryColor;
    e.secondaryColor = def->eggSecondaryColor;
    e.create         = def->create;
    e.tick           = def->tick;
    e.onHurt         = def->onHurt;
    e.onDeath        = def->onDeath;
    e.onSave         = def->onSave;
    e.onLoad         = def->onLoad;
    m_entities.push_back(std::move(e));

    char msg[512];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE,
        "Registered custom entity: %s", def->id);
    modLog("ModLoader", msg);
}

void* ModLoader::spawnEntity(const char* id,
                             void* level,
                             float x, float y, float z)
{
    if (!requirePermission("entities", "spawnEntity"))
        return nullptr;

    if (!id || !*id)
        return nullptr;

    for (const EntityEntry& e : m_entities)
    {
        if (_stricmp(e.id.c_str(), id) != 0 || !e.create)
            continue;

        void* handle = e.create(level, x, y, z);
        if (!handle)
            return nullptr;

        ActiveEntityEntry a;
        a.runtimeId = m_nextRuntimeEntityId++;
        a.typeId = e.id;
        a.handle = handle;
        a.level = level;
        a.x = x;
        a.y = y;
        a.z = z;
        a.tick = e.tick;
        a.onDeath = e.onDeath;
        a.onSave = e.onSave;
        m_activeEntities.push_back(a);

        char msg[512];
        _snprintf_s(msg, sizeof(msg), _TRUNCATE,
            "Spawned custom entity: %s #%d", e.id.c_str(), a.runtimeId);
        modLog("ModLoader", msg);

        char payload[1024];
        _snprintf_s(payload, sizeof(payload), _TRUNCATE,
            "spawn|%d|%s|%.3f|%.3f|%.3f",
            a.runtimeId, a.typeId.c_str(), a.x, a.y, a.z);
        broadcastToTracking(level, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z),
                            128, "modapi:entity_sync", payload, static_cast<int>(strlen(payload)));

        return handle;
    }

    return nullptr;
}

void ModLoader::tickActiveEntities()
{
    if (m_activeEntities.empty())
        return;

    std::vector<ActiveEntityEntry> next;
    next.reserve(m_activeEntities.size());

    for (ActiveEntityEntry& e : m_activeEntities)
    {
        if (!e.handle)
            continue;

        bool keep = true;
        float nx = e.x;
        float ny = e.y;
        float nz = e.z;
        if (e.tick)
            keep = e.tick(e.handle, e.level, &nx, &ny, &nz);

        if (!keep)
        {
            if (e.onDeath)
                e.onDeath(e.handle, nullptr);

            struct EntityDeathPayload { int runtimeId; const char* id; } deathPayload{ e.runtimeId, e.typeId.c_str() };
            emitEvent("entity.death", e.handle, &deathPayload, sizeof(deathPayload));

            std::wstring wid = widenAscii(e.typeId.c_str());
            applyEntityDrops(wid.c_str(), e.handle, e.level, false, 0);

            char payload[128];
            _snprintf_s(payload, sizeof(payload), _TRUNCATE, "remove|%d", e.runtimeId);
            broadcastToTracking(e.level, static_cast<int>(e.x), static_cast<int>(e.y), static_cast<int>(e.z),
                                128, "modapi:entity_sync", payload, static_cast<int>(strlen(payload)));
            continue;
        }

        e.x = nx;
        e.y = ny;
        e.z = nz;

        std::string syncBlob;
        if (e.onSave)
        {
            int need = e.onSave(e.handle, nullptr, 0);
            if (need > 0)
            {
                std::vector<unsigned char> buf(static_cast<size_t>(need));
                int wrote = e.onSave(e.handle, buf.data(), need);
                if (wrote > 0)
                    syncBlob = bytesToHex(buf.data(), wrote);
            }
        }

        char payload[1536];
        _snprintf_s(payload, sizeof(payload), _TRUNCATE,
            "update|%d|%.3f|%.3f|%.3f|%s",
            e.runtimeId, e.x, e.y, e.z, syncBlob.c_str());
        broadcastToTracking(e.level, static_cast<int>(e.x), static_cast<int>(e.y), static_cast<int>(e.z),
                            128, "modapi:entity_sync", payload, static_cast<int>(strlen(payload)));

        next.push_back(e);
    }

    m_activeEntities.swap(next);
}

void ModLoader::registerBlockRenderer(const ModBlockRendererDef* def)
{
    if (!requirePermission("rendering", "registerBlockRenderer"))
        return;

    if (!def || def->blockId <= 0 || !def->render)
        return;

    for (BlockRendererEntry& e : m_blockRenderers)
    {
        if (e.blockId == def->blockId)
        {
            e.render = def->render;
            return;
        }
    }

    BlockRendererEntry e;
    e.blockId = def->blockId;
    e.render = def->render;
    m_blockRenderers.push_back(e);
}

void ModLoader::registerItemRenderer(const ModItemRendererDef* def)
{
    if (!requirePermission("rendering", "registerItemRenderer"))
        return;

    if (!def || def->itemId <= 0 || !def->render)
        return;

    for (ItemRendererEntry& e : m_itemRenderers)
    {
        if (e.itemId == def->itemId)
        {
            e.render = def->render;
            return;
        }
    }

    ItemRendererEntry e;
    e.itemId = def->itemId;
    e.render = def->render;
    m_itemRenderers.push_back(e);
}

void ModLoader::registerEntityRenderer(const ModEntityRendererDef* def)
{
    if (!requirePermission("rendering", "registerEntityRenderer"))
        return;

    if (!def || !def->id || !*def->id || !def->render)
        return;

    for (EntityRendererEntry& e : m_entityRenderers)
    {
        if (_stricmp(e.id.c_str(), def->id) == 0)
        {
            e.render = def->render;
            return;
        }
    }

    EntityRendererEntry e;
    e.id = def->id;
    e.render = def->render;
    m_entityRenderers.push_back(e);
}

void ModLoader::registerProjectileRenderer(const ModProjectileRendererDef* def)
{
    if (!requirePermission("rendering", "registerProjectileRenderer"))
        return;

    if (!def || !def->id || !*def->id || !def->render)
        return;

    for (ProjectileRendererEntry& e : m_projectileRenderers)
    {
        if (_stricmp(e.id.c_str(), def->id) == 0)
        {
            e.render = def->render;
            return;
        }
    }

    ProjectileRendererEntry e;
    e.id = def->id;
    e.render = def->render;
    m_projectileRenderers.push_back(e);
}

void ModLoader::registerEventHandler(const char* eventName, ModEventHandlerFn fn)
{
    if (!requirePermission("events", "registerEventHandler"))
        return;

    if (!eventName || !*eventName || !fn)
        return;

    EventHandlerEntry e;
    e.eventName = eventName;
    e.fn = fn;
    m_eventHandlers.push_back(e);
}

void ModLoader::emitEvent(const ModEvent* e) const
{
    if (!requirePermission("events", "emitEvent"))
        return;

    if (!e || !e->name || !*e->name)
        return;

    for (const EventHandlerEntry& h : m_eventHandlers)
    {
        if (!h.fn)
            continue;
        if (_stricmp(h.eventName.c_str(), e->name) == 0)
            h.fn(e);
    }
}

void ModLoader::emitEvent(const char* eventName,
                         void* context,
                         const void* payload,
                         int payloadSize) const
{
    ModEvent e{};
    e.name = eventName;
    e.context = context;
    e.payload = payload;
    e.payloadSize = payloadSize;
    emitEvent(&e);
}

bool ModLoader::dispatchBlockRenderer(void* level,
                                      int x, int y, int z,
                                      int blockId,
                                      int blockData) const
{
    for (const BlockRendererEntry& e : m_blockRenderers)
    {
        if (e.blockId == blockId && e.render)
            return e.render(level, x, y, z, blockId, blockData);
    }
    return false;
}

bool ModLoader::dispatchItemRenderer(int itemId,
                                     int aux,
                                     int context,
                                     void* itemEntity) const
{
    for (const ItemRendererEntry& e : m_itemRenderers)
    {
        if (e.itemId == itemId && e.render)
            return e.render(itemId, aux, context, itemEntity);
    }
    return false;
}

bool ModLoader::dispatchEntityRenderer(void* entity,
                                       const char* entityId,
                                       float partialTick) const
{
    if (!entityId || !*entityId)
        return false;
    for (const EntityRendererEntry& e : m_entityRenderers)
    {
        if (_stricmp(e.id.c_str(), entityId) == 0 && e.render)
            return e.render(entity, entityId, partialTick);
    }
    return false;
}

bool ModLoader::dispatchProjectileRenderer(void* projectile,
                                           const char* projectileId,
                                           float partialTick) const
{
    if (!projectileId || !*projectileId)
        return false;
    for (const ProjectileRendererEntry& e : m_projectileRenderers)
    {
        if (_stricmp(e.id.c_str(), projectileId) == 0 && e.render)
            return e.render(projectile, projectileId, partialTick);
    }
    return false;
}

void ModLoader::renderActiveCustomEntities(float partialTick) const
{
    for (const ActiveEntityEntry& e : m_activeEntities)
        dispatchEntityRenderer(e.handle, e.typeId.c_str(), partialTick);
}

void ModLoader::renderActiveProjectiles(float partialTick) const
{
    for (const ActiveProjectileEntry& p : m_activeProjectiles)
        dispatchProjectileRenderer(p.handle, p.id.c_str(), partialTick);
}

void ModLoader::registerTexture(const char* id, const char* path)
{
    if (!requirePermission("assets", "registerTexture"))
        return;

    if (!id || !*id || !path || !*path)
        return;
    AssetEntry e;
    e.type = "texture";
    e.id   = id;
    e.path = path;
    m_assets.push_back(std::move(e));
    char msg[512];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Registered texture: %s -> %s", id, path);
    modLog("ModLoader", msg);
}

void ModLoader::registerSound(const char* id, const char* path)
{
    if (!requirePermission("assets", "registerSound"))
        return;

    if (!id || !*id || !path || !*path)
        return;
    AssetEntry e;
    e.type = "sound";
    e.id   = id;
    e.path = path;
    m_assets.push_back(std::move(e));
    char msg[512];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Registered sound: %s -> %s", id, path);
    modLog("ModLoader", msg);
}

void ModLoader::registerLang(const char* id, const char* path)
{
    if (!requirePermission("assets", "registerLang"))
        return;

    if (!id || !*id || !path || !*path)
        return;
    AssetEntry e;
    e.type = "lang";
    e.id   = id;
    e.path = path;
    m_assets.push_back(std::move(e));
    char msg[512];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Registered lang: %s -> %s", id, path);
    modLog("ModLoader", msg);
}

void ModLoader::registerModel(const char* id, const char* path)
{
    if (!requirePermission("assets", "registerModel"))
        return;

    if (!id || !*id || !path || !*path)
        return;
    AssetEntry e;
    e.type = "model";
    e.id   = id;
    e.path = path;
    m_assets.push_back(std::move(e));
    char msg[512];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Registered model: %s -> %s", id, path);
    modLog("ModLoader", msg);
}

// ---------------------------------------------------------------------------
// printLoadReport
// Emits a formatted summary to the debug output after all mods are loaded.
// ---------------------------------------------------------------------------

void ModLoader::printLoadReport() const
{
    OutputDebugStringA("========================================\n");
    OutputDebugStringA(" MeowMod Load Report\n");
    OutputDebugStringA("========================================\n");

    char line[1024];

    if (m_mods.empty())
    {
        OutputDebugStringA("  (no mods loaded)\n");
    }
    else
    {
        for (std::size_t i = 0; i < m_mods.size(); ++i)
        {
            const LoadedMod& m = m_mods[i];
            const char* displayId = m.modId.empty()       ? m.name.c_str() : m.modId.c_str();
            const char* dispName  = m.displayName.empty() ? m.name.c_str() : m.displayName.c_str();
            const char* ver       = m.version.empty()     ? "?"            : m.version.c_str();
            const char* author    = m.author.empty()      ? "?"            : m.author.c_str();
            _snprintf_s(line, sizeof(line), _TRUNCATE,
                "  [%zu] %s  v%s  by %s  (%s)\n",
                i + 1, dispName, ver, author, displayId);
            OutputDebugStringA(line);
            if (!m.description.empty())
            {
                _snprintf_s(line, sizeof(line), _TRUNCATE, "       %s\n", m.description.c_str());
                OutputDebugStringA(line);
            }
        }
    }

    _snprintf_s(line, sizeof(line), _TRUNCATE,
        "  Mods: %zu  |  Blocks: %zu  |  Items: %zu  |  Entities: %zu  |  Assets: %zu  |  Recipes: %zu  |  WorldGens: %zu  |  BlockCallbacks: %zu  |  TileEntities: %zu  |  GUIs: %zu  |  Commands: %zu  |  Structures: %zu  |  Projectiles: %zu  |  LightProviders: %zu\n",
        m_mods.size(), m_blocks.size(), m_items.size(),
        m_entities.size(), m_assets.size(),
        m_recipes.size(), m_worldGens.size(),
        m_blockCallbacks.size(), m_tileEntities.size(), m_guis.size(),
        m_commands.size(), m_structures.size(), m_projectiles.size(),
        m_dynamicLightQueries.size());
    OutputDebugStringA(line);
    OutputDebugStringA("========================================\n");
}
