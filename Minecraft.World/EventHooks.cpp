#include "stdafx.h"
#include "EventHooks.h"

void (*g_serverTick)(void)                             = nullptr;
void (*g_onLevelLoad)(int isServer)                    = nullptr;
void (*g_onLevelUnload)(int isServer)                  = nullptr;
void (*g_onPlayerJoin)(int entityId, const char* name) = nullptr;
void (*g_onPlayerLeave)(int entityId, const char* name)= nullptr;
