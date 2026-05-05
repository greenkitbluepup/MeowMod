#include "stdafx.h"
#include "LevelHooks.h"

int (*g_queryDynamicLight)(int x, int y, int z)   = nullptr;
int (*g_hostGetTileOpacity)(int x, int y, int z)  = nullptr;
void (*g_beginEmitterFeed)(void)                  = nullptr;
void (*g_notifyEmitter)(int entityId, int x, int y, int z, int strength) = nullptr;
void (*g_endEmitterFeed)(void)                    = nullptr;
void (*g_markRegionDirty)(int x0, int y0, int z0, int x1, int y1, int z1) = nullptr;
void (*g_notifyTileChanged)(int x, int y, int z)                             = nullptr;
