#include "stdafx.h"

#include "ModAPI.h"
#include "../Minecraft.World/ContentHooks.h"

#include "Minecraft.h"
#include "LocalPlayer.h"

#include "../Minecraft.World/Player.h"
#include "../Minecraft.World/Inventory.h"
#include "../Minecraft.World/ItemInstance.h"
#include "../Minecraft.World/Item.h"

#include <memory>

// ---------------------------------------------------------------------------
// Stack conversion helpers
// ---------------------------------------------------------------------------

static void ClearModStack(ModItemStack* out)
{
    if (!out)
        return;

    out->itemId = 0;
    out->aux = 0;
    out->count = 0;
}

static bool ToModStack(const std::shared_ptr<ItemInstance>& item, ModItemStack* out)
{
    if (!out)
        return false;

    if (!item || item->count <= 0)
    {
        ClearModStack(out);
        return true;
    }

    out->itemId = item->id;
    out->aux = item->getAuxValue();
    out->count = item->count;
    return true;
}

static std::shared_ptr<ItemInstance> FromModStack(const ModItemStack* stack)
{
    if (!stack || stack->itemId <= 0 || stack->count <= 0)
        return nullptr;

    return std::shared_ptr<ItemInstance>(
        new ItemInstance(stack->itemId, stack->count, stack->aux)
    );
}

// ---------------------------------------------------------------------------
// Player lookup
// ---------------------------------------------------------------------------

static void* HostGetLocalPlayer()
{
    Minecraft* mc = Minecraft::GetInstance();
    if (!mc)
        return nullptr;

    if (mc->player)
        return mc->player.get();

    if (mc->cameraTargetPlayer &&
        mc->cameraTargetPlayer->instanceof(eTYPE_LOCALPLAYER))
    {
        std::shared_ptr<LocalPlayer> lp =
            std::dynamic_pointer_cast<LocalPlayer>(mc->cameraTargetPlayer);

        if (lp)
            return lp.get();
    }

    for (int i = 0; i < XUSER_MAX_COUNT; ++i)
    {
        if (mc->localplayers[i])
            return mc->localplayers[i].get();
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// Main hand access
// ---------------------------------------------------------------------------

static bool HostGetPlayerSelectedItem(void* playerHandle, ModItemStack* out)
{
    if (!playerHandle || !out)
        return false;

    Player* player = static_cast<Player*>(playerHandle);
    if (!player || !player->inventory)
    {
        ClearModStack(out);
        return true;
    }

    return ToModStack(player->inventory->getSelected(), out);
}

static bool HostSetPlayerSelectedItem(void* playerHandle, const ModItemStack* stack)
{
    if (!playerHandle)
        return false;

    Player* player = static_cast<Player*>(playerHandle);
    if (!player || !player->inventory)
        return false;

    std::shared_ptr<ItemInstance> item = FromModStack(stack);
    player->inventory->setItem(player->inventory->selected, item);

    return true;
}

// ---------------------------------------------------------------------------
// Real inventory-owned offhand access
// ---------------------------------------------------------------------------

static bool HostGetPlayerOffhandItem(void* playerHandle, ModItemStack* out)
{
    if (!playerHandle || !out)
        return false;

    Player* player = static_cast<Player*>(playerHandle);
    if (!player || !player->inventory)
    {
        ClearModStack(out);
        return true;
    }

    return ToModStack(player->inventory->getOffhand(), out);
}

static bool HostSetPlayerOffhandItem(void* playerHandle, const ModItemStack* stack)
{
    if (!playerHandle)
        return false;

    Player* player = static_cast<Player*>(playerHandle);
    if (!player || !player->inventory)
        return false;

    player->inventory->setOffhand(FromModStack(stack));
    return true;
}

// ---------------------------------------------------------------------------
// Swap
// ---------------------------------------------------------------------------

static bool HostSwapPlayerHands(void* playerHandle)
{
    if (!playerHandle)
        return false;

    Player* player = static_cast<Player*>(playerHandle);
    if (!player || !player->inventory)
        return false;

    ModItemStack mainBefore{};
    ModItemStack offBefore{};

    HostGetPlayerSelectedItem(playerHandle, &mainBefore);
    HostGetPlayerOffhandItem(playerHandle, &offBefore);

    player->inventory->swapSelectedWithOffhand();

    char msg[256];
    _snprintf_s(
        msg,
        sizeof(msg),
        _TRUNCATE,
        "[OffhandHostBridge] swapped REAL inventory main item=%d count=%d with offhand item=%d count=%d\n",
        mainBefore.itemId,
        mainBefore.count,
        offBefore.itemId,
        offBefore.count
    );
    OutputDebugStringA(msg);

    return true;
}

// ---------------------------------------------------------------------------
// Installer
// ---------------------------------------------------------------------------

struct OffhandHostBridgeInstaller
{
    OffhandHostBridgeInstaller()
    {
        g_hostGetLocalPlayer = &HostGetLocalPlayer;

        g_hostGetPlayerSelectedItem = &HostGetPlayerSelectedItem;
        g_hostSetPlayerSelectedItem = &HostSetPlayerSelectedItem;

        g_hostGetPlayerOffhandItem = &HostGetPlayerOffhandItem;
        g_hostSetPlayerOffhandItem = &HostSetPlayerOffhandItem;

        g_hostSwapPlayerHands = &HostSwapPlayerHands;

        OutputDebugStringA("[OffhandHostBridge] installed\n");
    }

    ~OffhandHostBridgeInstaller()
    {
        g_hostGetLocalPlayer = nullptr;

        g_hostGetPlayerSelectedItem = nullptr;
        g_hostSetPlayerSelectedItem = nullptr;

        g_hostGetPlayerOffhandItem = nullptr;
        g_hostSetPlayerOffhandItem = nullptr;

        g_hostSwapPlayerHands = nullptr;

        OutputDebugStringA("[OffhandHostBridge] uninstalled\n");
    }
};

static OffhandHostBridgeInstaller g_offhandHostBridgeInstaller;