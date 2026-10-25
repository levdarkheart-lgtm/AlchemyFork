# -*- cmake -*-

if(NOT DARWIN OR NOT ARCH STREQUAL "arm64")
  message(FATAL_ERROR "almetal is an Apple Silicon-only backend")
endif()

find_library(AL_METAL_FRAMEWORK Metal REQUIRED)
find_library(AL_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
set(AL_METAL_BUILTIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/almetal_builtin.metal")
set(AL_METAL_BUILTIN_SOURCE "${AL_METAL_BUILTIN_SOURCE}" CACHE INTERNAL
    "Built-in native Metal shader source")

# Prefer a deterministic offline-compiled library in packaged viewers. Keep
# the source path below as a developer/test fallback for machines that have
# Xcode but not its separately installed Metal Toolchain component.
find_program(AL_XCRUN_EXECUTABLE NAMES xcrun)
set(al_metal_compiler_available FALSE)
if(AL_XCRUN_EXECUTABLE)
  execute_process(
    COMMAND "${AL_XCRUN_EXECUTABLE}" --sdk macosx --find metal
    RESULT_VARIABLE al_metal_compiler_result
    OUTPUT_QUIET
    ERROR_QUIET
  )
  execute_process(
    COMMAND "${AL_XCRUN_EXECUTABLE}" --sdk macosx --find metallib
    RESULT_VARIABLE al_metallib_result
    OUTPUT_QUIET
    ERROR_QUIET
  )
  if(al_metal_compiler_result EQUAL 0 AND al_metallib_result EQUAL 0)
    set(al_metal_compiler_available TRUE)
  endif()
endif()

if(al_metal_compiler_available)
  set(al_metal_builtin_dir "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/shaders")
  set(al_metal_builtin_air "${al_metal_builtin_dir}/almetal_builtin.air")
  set(AL_METAL_BUILTIN_LIBRARY "${al_metal_builtin_dir}/almetal_builtin.metallib")
  set(al_metal_deployment_arg "")
  if(CMAKE_OSX_DEPLOYMENT_TARGET)
    set(al_metal_deployment_arg
        "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif()
  add_custom_command(
    OUTPUT "${AL_METAL_BUILTIN_LIBRARY}"
    BYPRODUCTS "${al_metal_builtin_air}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${al_metal_builtin_dir}"
    COMMAND "${AL_XCRUN_EXECUTABLE}" --sdk macosx metal
            -std=metal3.1 -ffast-math ${al_metal_deployment_arg}
            -c "${AL_METAL_BUILTIN_SOURCE}" -o "${al_metal_builtin_air}"
    COMMAND "${AL_XCRUN_EXECUTABLE}" --sdk macosx metallib
            "${al_metal_builtin_air}" -o "${AL_METAL_BUILTIN_LIBRARY}"
    DEPENDS "${AL_METAL_BUILTIN_SOURCE}"
    COMMENT "Compiling built-in Metal shader library"
    VERBATIM
  )
  add_custom_target(almetal_builtin_metallib
                    DEPENDS "${AL_METAL_BUILTIN_LIBRARY}")
  set(AL_METAL_BUILTIN_LIBRARY "${AL_METAL_BUILTIN_LIBRARY}" CACHE INTERNAL
      "Offline-compiled built-in Metal shader library" FORCE)
  set(AL_METAL_BUILTIN_LIBRARY_TARGET almetal_builtin_metallib CACHE INTERNAL
      "Target that produces the built-in Metal shader library" FORCE)
else()
  message(WARNING
    "Metal Toolchain component unavailable; packaged viewer will use runtime shader compilation")
  set(AL_METAL_BUILTIN_LIBRARY "" CACHE INTERNAL
      "Offline-compiled built-in Metal shader library" FORCE)
  set(AL_METAL_BUILTIN_LIBRARY_TARGET "" CACHE INTERNAL
      "Target that produces the built-in Metal shader library" FORCE)
endif()

add_library(almetal STATIC)
target_sources(
  almetal
  PRIVATE
    almetaldevice.mm
    almetalframe.mm
    almetalpbrpreview.cpp
    almetalpipeline.mm
    almetalresource.mm
    almetalsculptpreview.cpp
    almetalsurface.mm
    almetalterrainpaint.cpp
    almetaltextureops.cpp
    almetaluirenderer.cpp
    almetalworld.cpp
  PUBLIC
    FILE_SET HEADERS
    FILES
      almetaldevice.h
      almetalframe.h
      almetalpbrpreview.h
      almetalpipeline.h
      almetalresource.h
      almetalsculptpreview.h
      almetalsurface.h
      almetalterrainpaint.h
      almetaltextureops.h
      almetaluirenderer.h
      almetalworld.h
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
if(AL_METAL_BUILTIN_LIBRARY_TARGET)
  add_dependencies(almetal ${AL_METAL_BUILTIN_LIBRARY_TARGET})
endif()

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
  if(AL_METAL_BUILTIN_LIBRARY)
    target_compile_definitions(
      almetal_tests
      PRIVATE AL_METAL_BUILTIN_LIBRARY_PATH="${AL_METAL_BUILTIN_LIBRARY}"
    )
  endif()
endif()
