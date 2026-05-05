add_library(DynamicLights SHARED
    DynamicLights.cpp
    DynamicLightManager.cpp
    DynamicLightManager.h
)

target_include_directories(DynamicLights PRIVATE
    "${CMAKE_SOURCE_DIR}/Minecraft.Mods"
)

# Match the engine's static CRT setting so the DLL resolves the same runtime
# symbols (/MTd in Debug, /MT in Release).
set_target_properties(DynamicLights PROPERTIES
    OUTPUT_NAME "DynamicLights"
    FOLDER "Mods"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

configure_compiler_target(DynamicLights)

# After build, copy DynamicLights.dll into the mods/ subdirectory next to the client exe
add_custom_command(TARGET DynamicLights POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:DynamicLights>"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/DynamicLights.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/mod.json"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/DynamicLights.json"
    COMMENT "Copying DynamicLights.dll and mod.json to mods/ output folder"
    VERBATIM
)
