#include "stdafx.h"
#include "ChunkBakeHooks.h"

void* (*g_prepareChunkLightSnapshot)(int chunkX, int chunkY, int chunkZ) = nullptr;
int   (*g_queryChunkSnapshotLight)(void* snapshot, int x, int y, int z)  = nullptr;
void  (*g_destroyChunkLightSnapshot)(void* snapshot)                      = nullptr;
