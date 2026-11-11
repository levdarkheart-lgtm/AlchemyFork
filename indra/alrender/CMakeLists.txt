# -*- cmake -*-

# Render infrastructure over llrender that holds no viewer state: what the
# pipeline's render resources and programs are built from and when they
# change, the far plane, the water's field, wave heights and stencil bits,
# the light map's blur, the depth under the HUD,
# snapshot cutouts and whether a spot light's shadow map is drawn again.
# Whatever reads settings or the scene hands these their inputs as values.

set(
  alrender_SOURCES
  alcutoutimage.cpp
  alfarplane.cpp
  allightmapblur.cpp
  alrenderinputrequests.cpp
  alrenderresourceplan.cpp
  alshaderinputs.cpp
  alspotshadowcache.cpp
  alwaterfield.cpp
  alwaterheightbake.cpp
)

set(
  alrender_HEADERS
  alcutoutimage.h
  alfarplane.h
  alframeend.h
  alhexlattice.h
  alhuddepth.h
  allightmapblur.h
  alrenderinputrequests.h
  alrenderresourceinputs.h
  alrenderresourceplan.h
  alshaderinputs.h
  alspotshadowcache.h
  alwaterfield.h
  alwaterheightbake.h
)

add_library(alrender STATIC)
target_sources(
  alrender
  PRIVATE ${alrender_SOURCES}
  PUBLIC FILE_SET HEADERS FILES ${alrender_HEADERS}
)

target_link_libraries(
  alrender
  PRIVATE al::flags ll::pocketfft
  PUBLIC llrender llimage llmath llcommon
)

target_precompile_headers(alrender REUSE_FROM llprecompiled)

if(AL_BUILD_TESTS)
  # The viewer's native shader source, also inspected by CPU contract checks.
  set(alrender_shader_dir
    "AL_TEST_METAL_SHADER=\"${INDRA_SOURCE_DIR}/almetal/shaders/almetal_builtin.metal\""
  )

  # The suites that need no GL: the render resources' plan and the changes
  # that ask for it again, what the programs are built from, the far plane
  # and the constants its shaders share, the water's field and the wave
  # heights a normal map's slopes integrate to, snapshot cutouts, the spot
  # shadows' cache, and how a display pass ends its frame.
  al_add_test_binary(
    alrender_tests
    PROJECT alrender
    SOURCES
      tests/alcutoutimage_test.cpp
      tests/alfarplane_test.cpp
      tests/alframeend_test.cpp
      tests/alrenderinputrequests_test.cpp
      tests/alrenderresourceplan_test.cpp
      tests/alrender_resource_invariants_test.cpp
      tests/alfxaa_native_invariants_test.mm
      tests/alshaderinputs_test.cpp
      tests/alspotshadowcache_test.cpp
      tests/alwaterfield_test.cpp
      tests/alwaterheightbake_test.cpp
    LIBRARIES alrender almetal
    SUPPORT llrender_test_checks llcommon_test_checks
    DEFINES ${alrender_shader_dir}
  )
  set_source_files_properties(tests/alfxaa_native_invariants_test.mm
                              PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

endif()
