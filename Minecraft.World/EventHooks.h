#pragma once

// EventHooks.h - Engine-side hook slots for the mod event bus.
//
// Zero dependency on Minecraft.Mods. ModLoader writes these slots after
// loading all mods. Engine call sites (server Level::tick, player manager,
// level lifecycle code) read and call through them.
//
// All slots are null until at least one mod registers a handler.

// Called once per tick on the dedicated-server game thread.
extern void (*g_serverTick)(void);

// Level lifecycle.
// isServer: 1 = dedicated server level, 0 = client level.
extern void (*g_onLevelLoad)(int isServer);
extern void (*g_onLevelUnload)(int isServer);

// Player events (server-side).
// entityId: stable in-world entity id.
// name    : player display name (UTF-8, engine-owned storage).
extern void (*g_onPlayerJoin)(int entityId, const char* name);
extern void (*g_onPlayerLeave)(int entityId, const char* name);
