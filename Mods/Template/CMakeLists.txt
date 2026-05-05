add_library(TemplateMod SHARED
add_library(TemplateMod SHARED
    TemplateModMain.cpp
)

target_include_directories(TemplateMod PRIVATE
    "${CMAKE_SOURCE_DIR}/Minecraft.Mods"
)

set_target_properties(TemplateMod PROPERTIES
    OUTPUT_NAME "TemplateMod"
    FOLDER "Mods"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

configure_compiler_target(TemplateMod)

add_custom_command(TARGET TemplateMod POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:TemplateMod>"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/TemplateMod.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/mod.json"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/TemplateMod.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/TemplateMod.cfg"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/TemplateMod.cfg"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/assets"
    COMMENT "Copying TemplateMod.dll, mod.json, .cfg, and assets manifest to mods/ output folder"
    VERBATIM
)
