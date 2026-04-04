#include "ModAPI.h"

// ---------------------------------------------------------------------------
// TemplateMod
//
// Copy this folder, rename the target in CMakeLists.txt, rename this file,
// and replace the sections below with your own logic.
// ---------------------------------------------------------------------------

static ModHostAPI g_api{};

// ---------------------------------------------------------------------------
// Client tick  (optional -- remove if unused)
// ---------------------------------------------------------------------------
// Called once per game tick on the game thread.
// Safe to call g_api.log() here, query state, etc.

static void OnClientTick()
{
    // Your per-tick logic here.
}

// ---------------------------------------------------------------------------
// Entry / exit
// ---------------------------------------------------------------------------

extern "C" __declspec(dllexport) bool InitMod(ModHostAPI* api)
{
    // Always check the version first.
    if (!api || api->apiVersion != MOD_API_VERSION)
        return false;

    g_api = *api;
    g_api.log("[TemplateMod] loaded");

    // Register only the callbacks you actually use.
    g_api.registerClientTick(&OnClientTick);

    return true;
}

extern "C" __declspec(dllexport) void ShutdownMod()
{
    // Free any resources allocated in InitMod or your callbacks.
}
