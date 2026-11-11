# -*- cmake -*-

if(NOT DARWIN OR NOT ARCH STREQUAL "arm64")
  message(FATAL_ERROR "almetal is an Apple Silicon-only backend")
endif()

find_library(AL_METAL_FRAMEWORK Metal REQUIRED)
find_library(AL_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(AL_IOSURFACE_FRAMEWORK IOSurface REQUIRED)
set(AL_METAL_BUILTIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/almetal_builtin.metal")
set(AL_METAL_BUILTIN_SOURCE "${AL_METAL_BUILTIN_SOURCE}" CACHE INTERNAL
    "Built-in native Metal shader source")
option(AL_METAL_RECORD_SHADER_SOURCES
       "Embed optimized Metal shader sources for local GPU capture inspection" OFF)

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
  set(al_metal_source_record_flags "")
  set(al_metal_shader_directory "shaders")
  if(AL_METAL_RECORD_SHADER_SOURCES)
    # Match Xcode's Include Source setting without reducing optimization.
    # Embedded text alone does not provide the line tables the profiler needs.
    list(APPEND al_metal_source_record_flags -gline-tables-only -frecord-sources)
    set(al_metal_shader_directory "shaders-with-source")
  endif()
  set(al_metal_builtin_dir "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${al_metal_shader_directory}")
  set(al_metal_builtin_air "${al_metal_builtin_dir}/almetal_builtin.air")
  set(AL_METAL_BUILTIN_LIBRARY "${al_metal_builtin_dir}/almetal_builtin.metallib")
  set(al_metal_deployment_arg "")
  if(CMAKE_OSX_DEPLOYMENT_TARGET)
    set(al_metal_deployment_arg
        "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif()
  set(al_metal_compile_options
      -std=metal3.1 -ffast-math ${al_metal_deployment_arg}
      ${al_metal_source_record_flags})
  # Xcode's script dependency check does not invalidate existing outputs when
  # only the command's options change. Make those options an actual input.
  set(al_metal_options_stamp "${al_metal_builtin_dir}/compiler-options.txt")
  file(GENERATE OUTPUT "${al_metal_options_stamp}"
       CONTENT "compiler=${AL_XCRUN_EXECUTABLE}\noptions=${al_metal_compile_options}\n")
  add_custom_command(
    OUTPUT "${AL_METAL_BUILTIN_LIBRARY}"
    BYPRODUCTS "${al_metal_builtin_air}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${al_metal_builtin_dir}"
    COMMAND "${AL_XCRUN_EXECUTABLE}" --sdk macosx metal
            ${al_metal_compile_options}
            -c "${AL_METAL_BUILTIN_SOURCE}" -o "${al_metal_builtin_air}"
    COMMAND "${AL_XCRUN_EXECUTABLE}" --sdk macosx metallib
            "${al_metal_builtin_air}" -o "${AL_METAL_BUILTIN_LIBRARY}"
    DEPENDS "${AL_METAL_BUILTIN_SOURCE}" "${al_metal_options_stamp}"
            "${CMAKE_CURRENT_LIST_FILE}"
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
    almetalavatarresources.cpp
    almetalframe.mm
    almetalframecapture.mm
    almetalpbrpreview.cpp
    almetalpipeline.mm
    almetalprobeworkload.mm
    almetalresource.mm
    almetalretainedbackground.mm
    almetalshadowindirect.mm
    almetalshadowmaterials.mm
    almetalstagetiming.mm
    almetalsculptpreview.cpp
    almetalsurface.mm
    almetalterrainpaint.cpp
    almetaltextureops.cpp
    almetaluirenderer.cpp
    almetalworld.cpp
    almetalworlddiagnostics.cpp
  PUBLIC
    FILE_SET HEADERS
    FILES
      almetaldevice.h
      almetalavatarresources.h
      almetalframe.h
      almetalframecapture.h
      almetalpbrpreview.h
      almetalpipeline.h
      almetalprobeworkload.h
      almetalresource.h
      almetalretainedbackground.h
      almetalshadowindirect.h
      almetalshadowmaterials.h
      almetalstagetiming.h
      almetalshadowcapture.h
      almetalsculptpreview.h
      almetalsurface.h
      almetalterrainpaint.h
      almetaltextureops.h
      almetaluirenderer.h
      almetalworld.h
      almetalworlddiagnostics.h
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
    ${AL_IOSURFACE_FRAMEWORK}
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
  almetalretainedbackground.mm
  almetalshadowindirect.mm
  almetalshadowmaterials.mm
  almetalstagetiming.mm
  almetalsurface.mm
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

if(AL_BUILD_TESTS)
  al_add_test_binary(
    almetal_tests
    PROJECT almetal
    SOURCES tests/almetal_command_timing_test.mm
            tests/almetal_retained_background_test.mm
            tests/almetal_classic_avatar_pose_test.mm
            tests/almetal_avatar_resources_test.mm tests/almetal_gpu_test.mm tests/almetal_encoder_cache_test.mm
            tests/almetal_indexed_join_test.mm
            tests/almetal_completion_test.mm tests/almetal_world_data_test.mm
            tests/almetal_borrowed_target_test.mm
            tests/almetal_base_lighting_fastpath_test.mm
            tests/almetal_debug_buffer_test.mm
            tests/almetal_shader_reload_test.mm
            tests/almetal_utility_reload_test.mm
            tests/almetal_preview_reload_test.mm
            tests/almetal_hero_overwrite_test.mm
            tests/almetal_shadow_indirect_test.mm
            tests/almetal_shadow_materials_test.mm
            tests/almetal_stage_timing_test.mm
            tests/almetal_shadow_capture_test.mm
            tests/almetal_shadow_gather_test.mm
            tests/almetal_world_diagnostics_test.cpp
            tests/almetal_gbuffer_constants_test.mm
            tests/almetal_probe_workload_test.mm
            tests/almetal_frame_capture_test.mm
            tests/almetal_function_constants_test.mm
    LIBRARIES almetal
    RESOURCE_LOCK apple_gpu
  )
  set_source_files_properties(tests/almetal_command_timing_test.mm tests/almetal_avatar_resources_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_retained_background_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_classic_avatar_pose_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_gpu_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_encoder_cache_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_indexed_join_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_completion_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_world_data_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_borrowed_target_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_base_lighting_fastpath_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_debug_buffer_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shader_reload_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_utility_reload_test.mm tests/almetal_preview_reload_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_hero_overwrite_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_indirect_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_materials_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_stage_timing_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_capture_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_gather_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_gbuffer_constants_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_probe_workload_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_frame_capture_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_function_constants_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  if(AL_METAL_BUILTIN_LIBRARY)
    target_compile_definitions(
      almetal_tests
      PRIVATE AL_METAL_BUILTIN_LIBRARY_PATH="${AL_METAL_BUILTIN_LIBRARY}"
    )
  endif()
endif()
