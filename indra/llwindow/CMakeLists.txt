# -*- cmake -*-

include(UI)

set(
  llwindow_BASE_SOURCES
  alwindowsdlheadless.cpp
  llcursortypes.cpp
  llkeyboard.cpp
  llkeyboardheadless.cpp
  llmousehandler.cpp
  llsdl.cpp
  llwindow.cpp
  llwindowheadless.cpp
  llwindowcallbacks.cpp
)

set(
  llwindow_BASE_HEADERS
  alwindowsdlheadless.h
  llcursortypes.h
  llkeyboard.h
  llkeyboardheadless.h
  llmousehandler.h
  llscrolldelta.h
  llsdl.h
  llwindow.h
  llwindowheadless.h
  llwindowcallbacks.h
)

set(
  llwindow_BASE_LINK_LIBRARIES
  llcommon
  llimage
  llmath
  llfilesystem
  llxml
)

add_library(llwindow STATIC)

target_sources(
  llwindow
  PRIVATE ${llwindow_BASE_SOURCES}
  PUBLIC FILE_SET HEADERS FILES ${llwindow_BASE_HEADERS} llpreeditor.h
)

# SDL video initialization serves both the native Metal window and hidden
# utility surface. Metal resources are shared directly by upload workers.
target_link_libraries(
  llwindow
  PRIVATE al::flags
  PUBLIC llrender ${llwindow_BASE_LINK_LIBRARIES} ll::uilibraries ll::SDL3core
)
target_precompile_headers(llwindow REUSE_FROM llprecompiled)

if(AL_USE_SDL_WINDOW)
  target_sources(
    llwindow
    PRIVATE llkeyboardsdl.cpp llwindowsdl.cpp
    PUBLIC FILE_SET HEADERS FILES llkeyboardsdl.h llwindowsdl.h
  )

  target_compile_definitions(llwindow PUBLIC LL_SDL_WINDOW=1)

  target_link_libraries(llwindow PUBLIC ll::SDL3)
endif()

if(DARWIN)
  target_link_libraries(llwindow PRIVATE almetal)

  # Objective-C++ helper that fixes up SDL's auto-created Cocoa menu bar
  # (strips the Cmd+W shortcut off its default Window > Close item); init_sdl
  # calls it.
  target_sources(llwindow PRIVATE llsdl_macos.mm PUBLIC FILE_SET HEADERS FILES llsdl_macos.h)

  # llwindow reuses llprecompiled's C++ PCH; that PCH has no Objective-C++
  # variant, so skip it for the .mm or CMake fails to resolve a cmake_pch.objcxx
  # header it expects this target to provide.
  set_source_files_properties(llsdl_macos.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

  # IOKit deployment-target shim, consumed by newview's llmachineid.cpp.
  target_sources(llwindow PUBLIC FILE_SET HEADERS FILES llwindowmacosx_iokit.h)
endif()
