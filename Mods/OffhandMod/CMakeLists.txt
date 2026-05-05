add_library(OffhandMod SHARED
    OffhandModMain.cpp
)

target_include_directories(OffhandMod PRIVATE
    "${CMAKE_SOURCE_DIR}/Minecraft.Mods"
)

set_target_properties(OffhandMod PROPERTIES
    OUTPUT_NAME "OffhandMod"
    FOLDER "Mods"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

configure_compiler_target(OffhandMod)

add_custom_command(TARGET OffhandMod POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:OffhandMod>"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/OffhandMod.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/mod.json"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/OffhandMod.json"
    COMMENT "Copying OffhandMod.dll and mod.json to mods/ output folder"
    VERBATIM
)