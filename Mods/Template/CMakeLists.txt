add_library(TemplateMod SHARED
    TemplateMod.cpp
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:TemplateMod>"
        "$<TARGET_FILE_DIR:Minecraft.Client>/mods/TemplateMod.dll"
    COMMENT "Copying TemplateMod.dll to mods/ output folder"
    VERBATIM
)
