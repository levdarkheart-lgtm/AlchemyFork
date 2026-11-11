# -*- cmake -*-

set(
  llrender_SOURCES
  alframefences.cpp
  algputimer.cpp
  almetalfetchedtexture.cpp
  alpacingslack.cpp
  alprimitiveplot.cpp
  alquerypool.cpp
  alrendercapabilities.cpp
  alsamplerstate.cpp
  altextureslot.cpp
  alfontcolrv1.cpp
  alfontface.cpp
  alfontshaping.cpp
  llcubemap.cpp
  llfontbitmapcache.cpp
  llfontfreetype.cpp
  llfontgl.cpp
  llfonttextcache.cpp
  llfontregistry.cpp
  llgl.cpp
  llglslshader.cpp
  llgltexture.cpp
  llimagegl.cpp
  llimagegl_upload_native.inl
  llrender.cpp
  llrender2dutils.cpp
  llrendernavprim.cpp
  llrendersphere.cpp
  llrendertarget.cpp
  llshadermgr.cpp
  lltexture.cpp
  lltexturemanagerbridge.cpp
  lluiimage.cpp
  llvertexbuffer.cpp
  llglcommonfunc.cpp
)

set(
  llrender_HEADERS
  alframefences.h
  algputimer.h
  almetalfetchedtexture.h
  alpacingslack.h
  alprimitiveplot.h
  alquerypool.h
  alrendercapabilities.h
  alrendertargetdesc.h
  alsamplerstate.h
  altextureslot.h
  alfontcolrv1.h
  alfontface.h
  alfontshaping.h
  llcubemap.h
  llfontgl.h
  llfonttextcache.h
  llfontfreetype.h
  llfontbitmapcache.h
  llfontregistry.h
  llgl.h
  llglheaders.h
  llglslshader.h
  llglstates.h
  llgltexture.h
  llgltypes.h
  llimagegl.h
  llrender.h
  llrender2dutils.h
  llrendernavprim.h
  llrendersphere.h
  llrendertarget.h
  llshadermgr.h
  lltexture.h
  lltexturemanagerbridge.h
  lluiimage.h
  lluiimage.inl
  llvertexbuffer.h
  llglcommonfunc.h
)

add_library(llrender STATIC)
target_sources(
  llrender
  PRIVATE ${llrender_SOURCES}
  PUBLIC FILE_SET HEADERS FILES ${llrender_HEADERS}
)

target_link_libraries(
  llrender
  PRIVATE al::flags
  PUBLIC
    llcommon
    llimage
    llmath
    llfilesystem
    llxml
    llwindow
    ll::freetype
    ll::harfbuzz
    ll::plutosvg
)

target_precompile_headers(llrender REUSE_FROM llprecompiled)

if(DARWIN)
  target_link_libraries(llrender PRIVATE almetal)
endif()

if(AL_BUILD_TESTS)
  # llrender pulls in llcommon / llimage / llmath / llfilesystem / llxml /
  # llwindow + ll::freetype / ll::harfbuzz / ll::plutosvg via
  # its own target_link_libraries; transitive linking propagates them to
  # consumers, so listing llrender alone is enough.

  # The bundled faces ship in the alchemy-fonts package; newview stages a
  # copy into its source tree only when the viewer itself is configured,
  # so the tests read the package directly. ALCHEMY_FONTS_DIR ends in
  # "fonts", the leaf LLFontGL::getFontPathLocal appends to the app dir,
  # so its parent stands in for the application directory.
  find_package(unofficial-alchemy-fonts CONFIG REQUIRED)
  get_filename_component(font_test_app_dir "${ALCHEMY_FONTS_DIR}" DIRECTORY)
  set(font_test_data_dir "${ALCHEMY_FONTS_DIR}/")
  set(font_test_fonts_xml "${INDRA_SOURCE_DIR}/newview/skins/default/xui/en/fonts.xml")
  set(startup_texture_test_dir "${INDRA_SOURCE_DIR}/newview/skins/default/textures")

  # The font directory reaches a test as a string literal. Tests that need a
  # real font file (Noto-COLRv1.ttf, InterVariable.woff2) locate it relative
  # to this dir; absent files cause the affected tests to skip rather than
  # fail, so a partial font set still runs the rest of the suite.
  set(
    font_defines
    "LLFONT_TEST_APP_DIR=\"${font_test_app_dir}\""
    "LLFONT_TEST_DATA_DIR=\"${font_test_data_dir}\""
    "LLFONT_TEST_FONTS_XML=\"${font_test_fonts_xml}\""
  )
  set(startup_texture_defines
      "AL_TEST_VIEWER_TEXTURE_DIR=\"${startup_texture_test_dir}\"")

  # The state guard's checks on what llrender keeps for the process, for
  # each of its test binaries.
  add_library(llrender_test_checks OBJECT tests/llrender_test_checks.cpp)
  target_link_libraries(llrender_test_checks PRIVATE al::flags al_test_support llrender)
  set_target_properties(llrender_test_checks PROPERTIES FOLDER "Tests/llrender")

  # The suites that need no GL: arithmetic (where a local coordinate lands
  # under the UI transform; the matrix stack, the projection helpers and the
  # clip-plane classes; the GL floor's version test), the font tables and
  # shaping, and render target descriptions. alfontshaping holds both Unicode tables against each other, so
  # this binary links ICU, which llrender itself has no need of.
  set(llrender_native_test_sources)
  if(DARWIN)
    list(APPEND llrender_native_test_sources
      tests/almetal_buffer_storage_test.cpp
      tests/aldebug_immediate_metal_test.cpp
      tests/albaking_shader_metal_test.cpp
      tests/alshader_native_lifecycle_test.cpp
      ../llappearanceutility/llbakingshadermgr.cpp
      tests/altextureslot_metal_test.cpp
      tests/alterraintexture_metal_test.cpp
      tests/algputimer_metal_test.cpp
      tests/alframefences_metal_test.mm
      tests/alquerypool_test.cpp
      tests/alquerypool_diagnostics_test.cpp
      tests/alquerypool_metal_diagnostics_test.cpp
      tests/alquerypool_metal_lifecycle_test.cpp
      tests/llvertexbuffer_test.cpp
      tests/alvertex_range_test.cpp
      tests/alvertex_draw_metal_test.cpp
      tests/alvertex_label_metal_test.mm
      tests/alvertex_publication_metal_test.cpp
      tests/alimagegl_metal_test.cpp
      tests/alimage_preview_native_test.cpp
      tests/alfont_atlas_metal_test.cpp
      tests/alfont_freetype_metal_test.cpp
      tests/alfont_colrv1_metal_test.cpp
      tests/alfont_registry_metal_test.cpp
      tests/alfont_shaping_metal_test.cpp
      tests/aluiimage_metal_test.cpp
      tests/alnative_ui_texture_binding_test.cpp
      tests/alhuddepth_metal_test.cpp
      tests/alraster_state_metal_test.cpp
      tests/alscenemonitor_ui_metal_test.cpp
      tests/alrender2dutils_metal_test.cpp
      tests/alrendersphere_metal_test.cpp
      tests/alinfiniteprojection_metal_test.mm
      tests/alrendertarget_native_test.mm
      tests/alimage_native_coverage_test.cpp
      tests/alimage_legacy_invariants_test.cpp
      tests/almetal_tessellation_invariants_test.mm
      tests/alprimitiveplot_test.cpp
    )
  endif()
  al_add_test_binary(
    llrender_tests
    PROJECT llrender
    SOURCES
      tests/alfontcolrv1_test.cpp
      tests/alfontshaping_test.cpp
      tests/alpacingslack_test.cpp
      tests/alrendercapabilities_test.cpp
      tests/alrenderstate_test.cpp
      tests/alrendertargetdesc_test.cpp
      tests/llfontbitmapcache_test.cpp
      tests/llfontfreetype_test.cpp
      tests/llfontregistry_test.cpp
      tests/llglfloor_test.cpp
      tests/llrendermatrix_test.cpp
      tests/lluitransform_test.cpp
      ${llrender_native_test_sources}
    LIBRARIES llrender ll::icu
    SUPPORT llrender_test_checks llcommon_test_checks
    DEFINES ${font_defines} ${startup_texture_defines}
  )
  if(DARWIN)
    target_link_libraries(llrender_tests PRIVATE almetal)
    set_source_files_properties(tests/alvertex_label_metal_test.mm
                                tests/almetal_tessellation_invariants_test.mm
                                tests/alframefences_metal_test.mm
                                tests/alinfiniteprojection_metal_test.mm
                                tests/alrendertarget_native_test.mm
                                PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    # Default-font getters keep process-lifetime pointers, as in the original
    # viewer. Give their native atlas/UI session a separate process instead
    # of mixing it with the scoped font-manager cases in llrender_tests.
    al_add_test_binary(
      llrender_font_ui_tests
      PROJECT llrender
      SOURCES
        tests/alfontgl_metal_test.cpp
        tests/alfonttextcache_metal_test.cpp
      LIBRARIES llrender
      SUPPORT llrender_test_checks llcommon_test_checks
      DEFINES ${font_defines}
    )
    target_link_libraries(llrender_font_ui_tests PRIVATE almetal)
  endif()

  if(DARWIN)
    # Compile the live hidden-window lifecycle invariant without registering
    # or linking it into any routinely executed test binary. It creates an
    # actual window when run, so offscreen renderer suites must not invoke it.
    add_library(llrender_manual_window_compile OBJECT tests/alwindowsdlheadless_test.cpp)
    target_link_libraries(llrender_manual_window_compile PRIVATE al::flags al_test_support llrender)
    target_include_directories(llrender_manual_window_compile PRIVATE ${INDRA_SOURCE_DIR}/test)
  endif()

endif()
