# -*- cmake -*-

include(PluginAPI)
include(LibVLCPlugin)

### media_plugin_libvlc - dedicated host executable that statically links the
### LibVLC media plugin together with the slplugin host driver (there is no
### dlopen'd plugin library any more).

add_executable(media_plugin_libvlc WIN32 MACOSX_BUNDLE)

target_sources(
  media_plugin_libvlc
  PRIVATE
    ${CMAKE_SOURCE_DIR}/llplugin/slplugin/slplugin.cpp
    ${CMAKE_SOURCE_DIR}/llplugin/slplugin/slplugin_static.cpp
    media_plugin_libvlc.cpp
    media_plugin_libvlc_lifecycle.inl
)

if(DARWIN)
  target_sources(media_plugin_libvlc PRIVATE ${CMAKE_SOURCE_DIR}/llplugin/slplugin/slplugin-objc.mm)
  set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/llplugin/slplugin/slplugin-objc.mm
    PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE
  )
endif()

if(AL_BUILD_TESTS)
  include(ALAddTestBinary)
  # The real lifecycle bodies and LLPluginMessage run against named backend
  # doubles. This target neither links libVLC nor launches the plugin host.
  al_add_test_binary(
    media_plugin_libvlc_tests
    PROJECT media_plugin_libvlc
    SOURCES tests/alvlc_lifecycle_test.cpp
    LIBRARIES llplugin llcommon
    SUPPORT llcommon_test_checks
    INCLUDES "$<TARGET_PROPERTY:unofficial::libvlc::libvlc,INTERFACE_INCLUDE_DIRECTORIES>"
  )
endif()

target_link_libraries(
  media_plugin_libvlc
  PRIVATE al::flags media_plugin_base ll::libvlc llplugin llmessage llcommon ll::pluginlibraries
)

if(WINDOWS)
  set_target_properties(
    media_plugin_libvlc
    PROPERTIES
      EXCLUDE_FROM_DEFAULT_BUILD_DEBUG ON
      RUNTIME_OUTPUT_DIRECTORY "${VIEWER_STAGING_DIR}/llplugin"
  )
  target_link_options(media_plugin_libvlc PRIVATE /MANIFEST:NO)
elseif(DARWIN)
  set_target_properties(
    media_plugin_libvlc
    PROPERTIES
      BUILD_WITH_INSTALL_RPATH 1
      INSTALL_RPATH
        "@executable_path/../../../../Frameworks;@executable_path/../Frameworks;@executable_path/../Frameworks/plugins"
  )
endif()
