# -*- cmake -*-

add_library(llimage STATIC)

target_sources(
  llimage
  PRIVATE
    alj2clayout.cpp
    llimagebmp.cpp
    llimage.cpp
    llimagedimensionsinfo.cpp
    llimagedxt.cpp
    llimagefilter.cpp
    llimagej2c.cpp
    llimagejpeg.cpp
    llimagepng.cpp
    llimagetga.cpp
    llimagewebp.cpp
    llimageavif.cpp
    llimageworker.cpp
    llpngwrapper.cpp
)

target_sources(
  llimage
  PUBLIC
    FILE_SET HEADERS
      FILES
        alimagedecodescheduler.h
        alj2clayout.h
        llimage.h
        llimagebmp.h
        llimagedimensionsinfo.h
        llimagedxt.h
        llimagefilter.h
        llimagej2c.h
        llimagejpeg.h
        llimagepng.h
        llimagetga.h
        llimagewebp.h
        llimageavif.h
        llimageworker.h
        llmapimagetype.h
        llpngwrapper.h
)

target_link_libraries(llimage PRIVATE al::flags)
if(AL_USE_KDU)
  target_link_libraries(llimage PUBLIC llkdu)
else()
  target_link_libraries(llimage PUBLIC llimagej2coj)
endif()

target_link_libraries(
  llimage
  PUBLIC llfilesystem llmath llcommon ll::libwebp ll::libavif ll::libpng ll::libjpeg
)

target_precompile_headers(llimage REUSE_FROM llprecompiled)

# Add tests
if(AL_BUILD_TESTS)
  # It stubs the image classes the library defines, so it compiles the one
  # file it tests instead of linking the library.
  al_add_test_binary(
    llimageworker_test
    PROJECT llimage
    SOURCES tests/llimageworker_test.cpp
    UNIT llimageworker.cpp
    LIBRARIES llmath llcommon
  )

  # The library itself: the locks images are read and written under, and the
  # J2C codestream layout and the byte targets it gives.
  set(al_j2c_experiment_tests "")
  if(AL_EXPERIMENTAL_OPENHTJ2K)
    list(APPEND al_j2c_experiment_tests tests/alopenhtj2k_test.cpp)
  endif()
  al_add_test_binary(
    llimage_tests
    PROJECT llimage
    SOURCES tests/llimage_test.cpp tests/alj2clayout_test.cpp tests/alimagedecodescheduler_test.cpp ${al_j2c_experiment_tests}
    LIBRARIES llimage ll::openjpeg
  )
endif()
