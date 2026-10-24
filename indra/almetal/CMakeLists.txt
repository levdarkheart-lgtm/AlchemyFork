# -*- cmake -*-

if(NOT DARWIN OR NOT ARCH STREQUAL "arm64")
  message(FATAL_ERROR "almetal is an Apple Silicon-only backend")
endif()

find_library(AL_METAL_FRAMEWORK Metal REQUIRED)
find_library(AL_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
set(AL_METAL_BUILTIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/almetal_builtin.metal")
set(AL_METAL_BUILTIN_SOURCE "${AL_METAL_BUILTIN_SOURCE}" CACHE INTERNAL
    "Built-in native Metal shader source")

add_library(almetal STATIC)
target_sources(
  almetal
  PRIVATE
    almetaldevice.mm
    almetalframe.mm
    almetalpipeline.mm
    almetalresource.mm
    almetalsculptpreview.cpp
    almetalsurface.mm
    almetalterrainpaint.cpp
    almetaltextureops.cpp
    almetaluirenderer.cpp
  PUBLIC
    FILE_SET HEADERS
    FILES
      almetaldevice.h
      almetalframe.h
      almetalpipeline.h
      almetalresource.h
      almetalsculptpreview.h
      almetalsurface.h
      almetalterrainpaint.h
      almetaltextureops.h
      almetaluirenderer.h
)
target_compile_definitions(
  almetal
  PRIVATE AL_METAL_BUILTIN_SOURCE_PATH="${AL_METAL_BUILTIN_SOURCE}"
)
target_link_libraries(
  almetal
  PRIVATE
    al::flags
    ${AL_METAL_FRAMEWORK}
    ${AL_QUARTZCORE_FRAMEWORK}
  PUBLIC
    ll::SDL3
)

# Objective-C++ sources cannot reuse the C++ precompiled header. Keep this
# explicit even though this small backend deliberately has no PCH of its own.
set_source_files_properties(
  almetaldevice.mm
  almetalframe.mm
  almetalpipeline.mm
  almetalresource.mm
  almetalsurface.mm
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

if(AL_BUILD_TESTS)
  al_add_test_binary(
    almetal_tests
    PROJECT almetal
    SOURCES tests/almetal_gpu_test.mm
    LIBRARIES almetal
    RESOURCE_LOCK apple_gpu
  )
  set_source_files_properties(tests/almetal_gpu_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()
