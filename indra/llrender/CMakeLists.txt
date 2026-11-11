# -*- cmake -*-

set(
  llrender_SOURCES
  alframefences.cpp
  algpubuffer.cpp
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
  aluniformbuffer.cpp
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
  algpubuffer.h
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
  aluniformbuffer.h
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
      tests/algpubuffer_test.cpp
      tests/aldebug_immediate_metal_test.cpp
      tests/albaking_shader_metal_test.cpp
      tests/alshader_native_lifecycle_test.cpp
      ../llappearanceutility/llbakingshadermgr.cpp
      tests/altextureslot_metal_test.cpp
      tests/alterraintexture_metal_test.cpp
      tests/algputimer_metal_test.cpp
      tests/alquerypool_test.cpp
      tests/alquerypool_diagnostics_test.cpp
      tests/alquerypool_metal_diagnostics_test.cpp
      tests/alquerypool_metal_lifecycle_test.cpp
      tests/aluniformbuffer_test.cpp
      tests/llvertexbuffer_test.cpp
      tests/alvertex_range_test.cpp
      tests/alvertex_draw_metal_test.cpp
      tests/alvertex_label_metal_test.mm
      tests/alvertex_publication_metal_test.cpp
      tests/alimagegl_metal_test.cpp
      tests/alfont_atlas_metal_test.cpp
      tests/alfont_freetype_metal_test.cpp
      tests/alfont_colrv1_metal_test.cpp
      tests/alfont_registry_metal_test.cpp
      tests/alfont_shaping_metal_test.cpp
      tests/aluiimage_metal_test.cpp
      tests/alrender2dutils_metal_test.cpp
      tests/alrendersphere_metal_test.cpp
      tests/alinfiniteprojection_metal_test.mm
      tests/alrendertarget_native_test.mm
      tests/alimage_native_coverage_test.cpp
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

  # The GL suites that stand a context up for each test case and end the GL
  # classes with it, on LLWindowManager's hidden window: the paths of the
  # font libraries above that rasterize, and llimagegl, which also makes a
  # context again in the middle of a case. Never beside the shared context
  # below, whose classes they would end under it.
  al_add_test_binary(
    llrender_gl_fresh_tests
    PROJECT llrender
    SOURCES
      tests/llimagegl_test.cpp
    LIBRARIES llrender ll::icu
    SUPPORT llrender_test_checks llcommon_test_checks
    DEFINES ${font_defines} ${startup_texture_defines}
    GL
  )

  # The GL suites that share one context for the process, each case putting
  # back what it changed of it:
  # - the hidden window itself, and a worker thread's shared context on it;
  # - tessellation stages and the quad conventions the terrain relies on;
  # - the infinite reverse-Z projection: stored depth, round trip and depth
  #   order out to 100 km;
  # - which fragment output reaches a render target's colour attachment,
  #   integer attachments cleared and drawn, targets kept, built again and
  #   re-pointed at a shared depth by their descriptions, and their labels;
  # - LLRender's cached clear colour, scissor box, viewport and raster state,
  #   LLGLSScissor and ALGLStencil;
  # - LLGLSLShader's cache key, its uniform value cache, the compute path
  #   (programs built through the shader manager, their images, storage
  #   blocks and dispatches), integer textures through reserved samplers,
  #   what the debug validator says of images and stencil reads, and which
  #   shader objects a program owns and which the manager does;
  # - the historical ALGPUBuffer store, readback and stream rings;
  # - ALUniformBuffer's persistently mapped rings: the store each makes, how
  #   it is mapped, what the GPU reads where a slice is bound, and the scratch
  #   ring's segments reused once the frame each was left in has finished;
  # - ALFrameFences: one fence a frame, polled at the frame's end, waits that
  #   stall only on frames the GPU has not finished, and the frame's end's
  #   pacing wait for the frame a given number before it, and no later one;
  # - ALQueryPool: each kind's names kept to its target, a held target
  #   refused, results read once their frame has ended, spans, release and
  #   conditional draws;
  # - LLVertexBuffer: what a buffer draws straight after an upload, after a
  #   second upload and after a new size, and that an upload sends only the
  #   stores its map wrote; and LLRenderSphere's buffer drawn under the debug
  #   GL checks.
  al_add_test_binary(
    llrender_gl_shared_tests
    PROJECT llrender
    SOURCES
      tests/alframefences_gl_test.cpp
      tests/alrenderstate_gl_test.cpp
      tests/altessellation_gl_test.cpp
      tests/aluniformbuffer_gl_test.cpp
      tests/alwindowsdlheadless_test.cpp
    LIBRARIES llrender
    SUPPORT llrender_test_checks llcommon_test_checks
    DEFINES ${font_defines}
    GL
  )
endif()
