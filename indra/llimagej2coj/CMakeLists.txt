# -*- cmake -*-

add_library(llimagej2coj STATIC)

target_sources(llimagej2coj PRIVATE llimagej2coj.cpp PUBLIC FILE_SET HEADERS FILES llimagej2coj.h)

target_link_libraries(llimagej2coj PRIVATE al::flags PUBLIC llimage llcommon ll::openjpeg)

include(OpenHTJ2KExperiment)
if(AL_EXPERIMENTAL_OPENHTJ2K)
  target_link_libraries(llimagej2coj PRIVATE ll::openhtj2k)
  target_compile_definitions(llimagej2coj PRIVATE AL_EXPERIMENTAL_OPENHTJ2K=1)
endif()

target_precompile_headers(llimagej2coj REUSE_FROM llprecompiled)
