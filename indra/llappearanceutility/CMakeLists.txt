# -*- cmake -*-

set(
  appearance_utility_SOURCES
  appearance_utility.cpp
  llappappearanceutility.cpp
  llbakingavatar.cpp
  llbakingjoint.cpp
  llbakingjointmesh.cpp
  llbakingprocess.cpp
  llbakingshadermgr.cpp
  llbakingtexlayer.cpp
  llbakingtexture.cpp
  llbakingwearable.cpp
  llbakingwearablesdata.cpp
  llbakingwindow.cpp
  llprocessparams.cpp
  llprocessskin.cpp
  llprocesstexture.cpp
)

set(
  appearance_utility_HEADERS
  llappappearanceutility.h
  llbakingavatar.h
  llbakingjoint.h
  llbakingjointmesh.h
  llbakingprocess.h
  llbakingshadermgr.h
  llbakingtexlayer.h
  llbakingtexture.h
  llbakingwearable.h
  llbakingwearablesdata.h
  llbakingwindow.h
  llprocessparams.h
  llprocessskin.h
  llprocesstexture.h
)

add_executable(appearance-utility-bin)

target_sources(
  appearance-utility-bin
  PRIVATE ${appearance_utility_SOURCES} ${appearance_utility_HEADERS}
)

target_link_libraries(
  appearance-utility-bin
  PRIVATE al::flags llcharacter llinventory llappearance llwindow llrender llxml
)
if(DARWIN)
  # The Apple Silicon utility consumes the native offscreen capture API
  # directly; do not rely on llrender/llwindow's private implementation link
  # to publish either the headers or the final executable dependency.
  target_link_libraries(appearance-utility-bin PRIVATE almetal)
  # Plain command-line NSBundle lookup uses the executable directory. Stage
  # the same actual native artifacts loadBuiltin selects, rather than relying
  # on the development source-tree fallback or displaced GLSL assets.
  set_property(TARGET appearance-utility-bin APPEND PROPERTY LINK_DEPENDS
               "${AL_METAL_BUILTIN_SOURCE}"
               "${INDRA_SOURCE_DIR}/almetal/THIRD_PARTY_NOTICES.txt")
  if(AL_METAL_BUILTIN_LIBRARY_TARGET)
    add_dependencies(appearance-utility-bin ${AL_METAL_BUILTIN_LIBRARY_TARGET})
  endif()
  add_custom_command(TARGET appearance-utility-bin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AL_METAL_BUILTIN_SOURCE}"
            "${INDRA_SOURCE_DIR}/almetal/THIRD_PARTY_NOTICES.txt"
            $<TARGET_FILE_DIR:appearance-utility-bin>
    VERBATIM)
  if(AL_METAL_BUILTIN_LIBRARY)
    set_property(TARGET appearance-utility-bin APPEND PROPERTY
                 LINK_DEPENDS "${AL_METAL_BUILTIN_LIBRARY}")
    add_custom_command(TARGET appearance-utility-bin POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${AL_METAL_BUILTIN_LIBRARY}"
              $<TARGET_FILE_DIR:appearance-utility-bin>
      VERBATIM)
  endif()
endif()

# What the utility reads at run time, beside the executable as the viewer
# has it: avatar definitions from the viewer's own source tree. Native shader
# artifacts and their notices are staged above.
add_custom_command(
  TARGET appearance-utility-bin
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory_if_different ${INDRA_SOURCE_DIR}/newview/character
    $<TARGET_FILE_DIR:appearance-utility-bin>/character
  COMMENT "Staging the appearance utility's data beside it"
  VERBATIM
)
