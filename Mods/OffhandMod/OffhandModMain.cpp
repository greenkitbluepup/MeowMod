#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ModAPI.h"

#include <cstring>

static ModHostAPI g_api{};

static bool g_prevFDown = false;
static bool g_requestedInitialSync = false;

static const char* kSwapChannel = "offhandmod:swap";
static const char* kRequestSyncChannel = "offhandmod:request_sync";
static const char* kSyncChannel = "offhandmod:sync";

static void SendOffhandSyncToClient(void* player)
{
    if (!player || !g_api.getPlayerOffhandItem || !g_api.sendToClient)
        return;

    ModItemStack offhand{};
    if (!g_api.getPlayerOffhandItem(player, &offhand))
        return;

    g_api.sendToClient(
        player,
        kSyncChannel,
        &offhand,
        sizeof(offhand)
    );
}

static void ApplyOffhandSyncClient(const void* payload, int size)
{
    if (!payload || size < static_cast<int>(sizeof(ModItemStack)))
        return;

    if (!g_api.getLocalPlayer || !g_api.setPlayerOffhandItem)
        return;

    void* player = g_api.getLocalPlayer();
    if (!player)
        return;

    const ModItemStack* stack = static_cast<const ModItemStack*>(payload);

    if (g_api.setPlayerOffhandItem(player, stack))
    {
        if (g_api.log)
            g_api.log("[OffhandMod] client applied offhand sync");
    }
}

static void SwapClientSidePrediction()
{
    if (!g_api.getLocalPlayer || !g_api.swapPlayerHands)
        return;

    void* player = g_api.getLocalPlayer();
    if (!player)
        return;

    if (g_api.swapPlayerHands(player))
    {
        if (g_api.log)
            g_api.log("[OffhandMod] client F swap prediction succeeded");
    }
    else
    {
        if (g_api.log)
            g_api.log("[OffhandMod] client F swap prediction failed");
    }
}

static void RequestInitialSyncIfReady()
{
    if (g_requestedInitialSync)
        return;

    if (!g_api.getLocalPlayer || !g_api.sendToServer)
        return;

    void* player = g_api.getLocalPlayer();
    if (!player)
        return;

    const char payload[] = "sync";

    if (g_api.sendToServer(kRequestSyncChannel, payload, sizeof(payload)))
    {
        g_requestedInitialSync = true;

        if (g_api.log)
            g_api.log("[OffhandMod] requested initial offhand sync");
    }
}

static void OnClientTick()
{
    RequestInitialSyncIfReady();

    bool fDown = (GetAsyncKeyState('F') & 0x8000) != 0;

    if (fDown && !g_prevFDown)
    {
        const char payload[] = "swap";

        if (g_api.sendToServer)
        {
            if (g_api.sendToServer(kSwapChannel, payload, sizeof(payload)))
            {
                if (g_api.log)
                    g_api.log("[OffhandMod] sent swap packet to server");
            }
            else
            {
                if (g_api.log)
                    g_api.log("[OffhandMod] failed to send swap packet to server");
            }
        }

        // Client prediction so the swap feels instant.
        // Server will send an authoritative offhand sync afterward.
        SwapClientSidePrediction();
    }

    g_prevFDown = fDown;
}

static void OnServerTick()
{
}

static bool OnServerPacket(void* sender, const char* channel, const void* payload, int size)
{
    (void)payload;
    (void)size;

    if (!channel)
        return false;

    if (std::strcmp(channel, kRequestSyncChannel) == 0)
    {
        if (g_api.log)
            g_api.log("[OffhandMod] server received offhand sync request");

        SendOffhandSyncToClient(sender);
        return true;
    }

    if (std::strcmp(channel, kSwapChannel) == 0)
    {
        if (!sender)
        {
            if (g_api.log)
                g_api.log("[OffhandMod] server swap failed: sender missing");

            return true;
        }

        if (!g_api.swapPlayerHands)
        {
            if (g_api.log)
                g_api.log("[OffhandMod] server swap failed: swapPlayerHands missing");

            return true;
        }

        if (g_api.swapPlayerHands(sender))
        {
            if (g_api.log)
                g_api.log("[OffhandMod] server swap succeeded");

            // Send authoritative offhand state back after swap.
            SendOffhandSyncToClient(sender);
        }
        else
        {
            if (g_api.log)
                g_api.log("[OffhandMod] server swap failed");
        }

        return true;
    }

    return false;
}

static bool OnClientPacket(const char* channel, const void* payload, int size)
{
    if (!channel)
        return false;

    if (std::strcmp(channel, kSyncChannel) != 0)
        return false;

    ApplyOffhandSyncClient(payload, size);
    return true;
}

static void OnAnyEvent(const ModEvent* e)
{
    (void)e;
}

extern "C" __declspec(dllexport) bool InitMod(ModHostAPI* api)
{
    if (!api)
        return false;

    if (api->apiVersion != MOD_API_VERSION)
        return false;

    g_api = *api;

    if (g_api.log)
        g_api.log("[OffhandMod] InitMod");

    if (!g_api.getLocalPlayer && g_api.log)
        g_api.log("[OffhandMod] ERROR: getLocalPlayer missing");

    if (!g_api.swapPlayerHands && g_api.log)
        g_api.log("[OffhandMod] ERROR: swapPlayerHands missing");

    if (!g_api.sendToServer && g_api.log)
        g_api.log("[OffhandMod] ERROR: sendToServer missing");

    if (!g_api.sendToClient && g_api.log)
        g_api.log("[OffhandMod] ERROR: sendToClient missing");

    if (!g_api.registerServerPacketHandler && g_api.log)
        g_api.log("[OffhandMod] ERROR: registerServerPacketHandler missing");

    if (!g_api.registerClientPacketHandler && g_api.log)
        g_api.log("[OffhandMod] ERROR: registerClientPacketHandler missing");

    if (g_api.registerClientTick)
        g_api.registerClientTick(&OnClientTick);

    if (g_api.registerServerTick)
        g_api.registerServerTick(&OnServerTick);

    if (g_api.registerServerPacketHandler)
        g_api.registerServerPacketHandler(&OnServerPacket);

    if (g_api.registerClientPacketHandler)
        g_api.registerClientPacketHandler(&OnClientPacket);

    if (g_api.registerEventHandler)
    {
        g_api.registerEventHandler("player.tick", &OnAnyEvent);
        g_api.registerEventHandler("player.use_item", &OnAnyEvent);
        g_api.registerEventHandler("client.render_frame", &OnAnyEvent);
    }

    if (g_api.log)
        g_api.log("[OffhandMod] hooks registered");

    return true;
}

extern "C" __declspec(dllexport) void ShutdownMod()
{
    if (g_api.log)
        g_api.log("[OffhandMod] ShutdownMod");

    g_api = {};
    g_prevFDown = false;
    g_requestedInitialSync = false;
}