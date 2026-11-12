# -*- cmake -*-

add_library(llaudio STATIC)

target_sources(
  llaudio
  PRIVATE llaudioengine.cpp lllistener.cpp llaudiodecodemgr.cpp llvorbisencode.cpp llwavfile.cpp
)

target_sources(
  llaudio
  PUBLIC
    FILE_SET HEADERS
      FILES llaudioengine.h lllistener.h llaudiodecodemgr.h llvorbisencode.h llwavfile.h llwindgen.h
)

target_link_libraries(
  llaudio
  PRIVATE al::flags
  PUBLIC llcommon llmath llmessage llfilesystem ll::vorbis ll::openal ll::fmodstudio ll::faudio
)

target_precompile_headers(llaudio REUSE_FROM llprecompiled)

# The audio engines. Each ll:: target is empty when its option is off, so
# only the sources are gated.
if(AL_USE_OPENAL)
  target_sources(
    llaudio
    PRIVATE llaudioengine_openal.cpp lllistener_openal.cpp
    PUBLIC FILE_SET HEADERS FILES llaudioengine_openal.h lllistener_openal.h
  )
endif()

if(AL_USE_FMODSTUDIO)
  target_sources(
    llaudio
    PRIVATE llaudioengine_fmodstudio.cpp lllistener_fmodstudio.cpp llstreamingaudio_fmodstudio.cpp
    PUBLIC
      FILE_SET HEADERS
        FILES llaudioengine_fmodstudio.h lllistener_fmodstudio.h llstreamingaudio_fmodstudio.h
  )
endif()

if(AL_USE_FAUDIO)
  target_sources(
    llaudio
    PRIVATE llaudioengine_faudio.cpp lllistener_faudio.cpp llcoreaudiooutputdevice.cpp
    PUBLIC FILE_SET HEADERS FILES llaudioengine_faudio.h lllistener_faudio.h llcoreaudiooutputdevice.h
  )
endif()

# The startup compatibility route is native too, including builds selecting
# another sound-effects engine. It must not depend on FAudio being enabled.
if(DARWIN)
  target_sources(
    llaudio
    PRIVATE llstreamingaudio_coreaudio.mm
    PUBLIC FILE_SET HEADERS FILES llstreamingaudio_coreaudio.h
  )
  set_source_files_properties(llstreamingaudio_coreaudio.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  target_link_libraries(llaudio PRIVATE "-framework Foundation" "-framework AudioToolbox" "-framework CoreAudio" "-framework AVFoundation" "-framework CoreMedia")
  if(AL_BUILD_TESTS)
    set_source_files_properties(tests/llstreamingaudio_coreaudio_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    al_add_test_binary(
      llaudio_coreaudio_tests
      PROJECT llaudio
      SOURCES tests/llstreamingaudio_coreaudio_test.mm
      LIBRARIES llaudio
      SUPPORT llcommon_test_checks
    )
  endif()
endif()
