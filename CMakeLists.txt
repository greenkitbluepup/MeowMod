cmake_minimum_required(VERSION 3.24)

project(MinecraftConsoles LANGUAGES C CXX RC ASM_MASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(FATAL_ERROR "This CMake build currently supports Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Use a 64-bit generator/toolchain (x64).")
endif()

set(CMAKE_CONFIGURATION_TYPES
  "Debug"
  "Release"
  CACHE STRING "" FORCE
)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

function(configure_compiler_target target)
  # MSVC and compatible compilers (like Clang-cl)
  if (MSVC)
    target_compile_options(${target} PRIVATE
      $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W3>
      $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W0>
      $<$<COMPILE_LANGUAGE:C,CXX>:/MP>
      $<$<COMPILE_LANGUAGE:C,CXX>:/FS>
      $<$<COMPILE_LANGUAGE:C,CXX>:/GS>
      $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
      $<$<COMPILE_LANGUAGE:CXX>:/GR>
      $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>
      $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2 /Oi /GT /GF>
    )

    # When the build shell has %LIB% pointing at x86 trees (e.g. invoked
    # without vcvars64), the linker falls back to x86 CRT/SDK libs and fails
    # with LNK4272. Derive the x64 lib paths from the compiler location, which
    # CMake always resolves correctly, and inject them so every target links
    # against the right architecture regardless of the shell environment.
    get_filename_component(_vc_bin   "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_vc_host  "${_vc_bin}"            DIRECTORY)
    get_filename_component(_vc_bin2  "${_vc_host}"           DIRECTORY)
    get_filename_component(_vc_tools "${_vc_bin2}"           DIRECTORY)

    set(_winsdk_um_x64   "")
    set(_winsdk_ucrt_x64 "")
    if(CMAKE_RC_COMPILER)
      get_filename_component(_rc_x64    "${CMAKE_RC_COMPILER}" DIRECTORY)
      get_filename_component(_rc_ver    "${_rc_x64}"           DIRECTORY)
      get_filename_component(_rc_bin    "${_rc_ver}"           DIRECTORY)
      get_filename_component(_kits_root "${_rc_bin}"           DIRECTORY)
      get_filename_component(_sdk_ver   "${_rc_ver}"           NAME)
      set(_winsdk_um_x64   "${_kits_root}/lib/${_sdk_ver}/um/x64")
      set(_winsdk_ucrt_x64 "${_kits_root}/lib/${_sdk_ver}/ucrt/x64")
    endif()

    target_link_options(${target} PRIVATE
      "/LIBPATH:${_vc_tools}/lib/x64"
      "/LIBPATH:${_vc_tools}/atlmfc/lib/x64"
      $<$<BOOL:${_winsdk_um_x64}>:/LIBPATH:${_winsdk_um_x64}>
      $<$<BOOL:${_winsdk_ucrt_x64}>:/LIBPATH:${_winsdk_ucrt_x64}>
    )
  endif()

  # MSVC
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${target} PRIVATE
      $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GL>
    )
    target_link_options(${target} PRIVATE
      $<$<CONFIG:Release>:/LTCG:incremental>
    )
  endif()

  # Clang
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${target} PRIVATE
      $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:-O0 -Wall>
      $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:-O2 -w -flto>
    )
    target_link_options(${target} PRIVATE
      $<$<CONFIG:Release>:-flto>
    )
  endif()
endfunction()


# ---
# Configuration
# ---
set(MINECRAFT_SHARED_DEFINES
  _LARGE_WORLDS
  _DEBUG_MENUS_ENABLED
  $<$<CONFIG:Debug>:_DEBUG>
  _CRT_NON_CONFORMING_SWPRINTFS
  _CRT_SECURE_NO_WARNINGS
)

# Add platform-specific defines
list(APPEND MINECRAFT_SHARED_DEFINES ${PLATFORM_DEFINES})

# ---
# Sources
# ---
add_subdirectory(Minecraft.World)
add_subdirectory(Minecraft.Mods)
add_subdirectory(Minecraft.Client)

if(PLATFORM_NAME STREQUAL "Windows64")
  add_subdirectory(Mods/DynamicLights)
  # add_subdirectory(Mods/Template)   # uncomment to build the template mod
endif()
if(PLATFORM_NAME STREQUAL "Windows64") # Server is only supported on Windows for now
  add_subdirectory(Minecraft.Server)
endif()

# ---
# Build versioning
# ---
set(BUILDVER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildVer.cmake")
set(BUILDVER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/Common/BuildVer.h")

add_custom_target(GenerateBuildVer
  COMMAND ${CMAKE_COMMAND}
    "-DOUTPUT_FILE=${BUILDVER_OUTPUT}"
    -P "${BUILDVER_SCRIPT}"
  COMMENT "Generating BuildVer.h..."
  VERBATIM
)

add_dependencies(Minecraft.World GenerateBuildVer)
add_dependencies(Minecraft.Mods GenerateBuildVer)
add_dependencies(Minecraft.Client GenerateBuildVer)
if(PLATFORM_NAME STREQUAL "Windows64")
  add_dependencies(Minecraft.Server GenerateBuildVer)
endif()

# ---
# Project organisation
# ---
# Set the startup project for Visual Studio
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Minecraft.Client)

# Setup folders for Visual Studio, just hides the build targets under a sub folder
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET GenerateBuildVer PROPERTY FOLDER "Build")
set_property(TARGET Minecraft.Mods   PROPERTY FOLDER "Engine")
