#pragma once

// initModContentRegistry() -- call once, early in engine startup, before
// Tile::staticCtor() runs.  Sets all g_host* slots in ContentHooks so that
// ModLoader can create tiles, items, and recipes through them.
void initModContentRegistry();
