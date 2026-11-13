# -*- cmake -*-

# The one wrapper of the vendor library. libwebrtc ships no Debug build on
# Windows, so this target is not built for that configuration; ll::webrtc
# tells the viewer so.
add_library(llwebrtc STATIC)

target_sources(
  llwebrtc
  PRIVATE
    alaudiodevicenotifier.cpp
    alaudioechobuffer.cpp
    llwebrtc.cpp
    alaudiodevicenotifier.h
    alaudioechobuffer.h
    llwebrtc_impl.h
  PUBLIC FILE_SET HEADERS FILES llwebrtc.h
)

target_link_libraries(llwebrtc PRIVATE al::flags)

# WebRTC is built with RTTI disabled (-fno-rtti / /GR-). Subclasses such as
# LLWebRTCLogSink / LLCustomProcessor need matching flags or the linker looks
# for typeinfo symbols that libwebrtc.a does not provide.
if(WINDOWS)
  target_compile_options(llwebrtc PRIVATE /GR-)
elseif(DARWIN)
  # PPD-091: Apple Clang's no-RTTI exception tables otherwise define private
  # std::exception typeinfo in this static archive. Once linked into the viewer,
  # that copy breaks base catches for runtime errors thrown by system libc++.
  # Keep the vendor-facing wrapper a no-RTTI/no-exception boundary; callers,
  # image codecs and all other viewer targets retain normal exception handling.
  # Allocation/container contract failures inside this boundary are fail-fast.
  target_compile_options(llwebrtc PRIVATE -fno-rtti -fno-exceptions)
else()
  target_compile_options(llwebrtc PRIVATE -fno-rtti)
endif()

if(AL_BUILD_TESTS AND DARWIN)
  # Exercise the production voice wrapper and image codec in one executable:
  # their exception/typeinfo link interaction is not covered by isolated tests.
  al_add_test_binary(
    alwebrtc_exception_abi_tests
    PROJECT llwebrtc
    SOURCES tests/alwebrtc_exception_abi_test.cpp
    LIBRARIES llwebrtc llimage
  )
endif()

target_link_libraries(llwebrtc PRIVATE unofficial::webrtc::webrtc)

if(WINDOWS)
  set_target_properties(llwebrtc PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG ON)
  target_link_libraries(
    llwebrtc
    PRIVATE secur32 winmm dmoguids wmcodecdspuuid msdmo strmiids iphlpapi ole32
  )
elseif(DARWIN)
  target_link_libraries(
    llwebrtc
    PRIVATE
      ll::oslibraries
      "-framework AVFoundation"
      "-framework AudioToolbox"
      "-framework CoreAudio"
      "-framework CoreMedia"
      "-framework CoreVideo"
      "-framework Foundation"
  )
  target_compile_options(llwebrtc PRIVATE -Wno-deprecated-declarations) # webrtc::CreateAudioDeviceWithDataObserver is deprecated
else()
  target_link_libraries(llwebrtc PRIVATE ${CMAKE_DL_LIBS})
  target_compile_options(llwebrtc PRIVATE -Wno-deprecated-declarations) # webrtc::CreateAudioDeviceWithDataObserver is deprecated
endif()
