add_library(Minecraft.Mods STATIC
    ModAPI.h
    ModLoader.h
    ModLoader.cpp
)

target_include_directories(Minecraft.Mods PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(Minecraft.Mods PRIVATE
    ${MINECRAFT_SHARED_DEFINES}
)

configure_compiler_target(Minecraft.Mods)

set_target_properties(Minecraft.Mods PROPERTIES FOLDER "Engine")
