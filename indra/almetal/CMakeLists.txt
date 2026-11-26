# -*- cmake -*-

if(NOT DARWIN OR NOT ARCH STREQUAL "arm64")
  message(FATAL_ERROR "almetal is an Apple Silicon-only backend")
endif()

find_library(AL_METAL_FRAMEWORK Metal REQUIRED)
find_library(AL_METALFX_FRAMEWORK MetalFX REQUIRED)
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
      -std=metal3.1 -ffast-math -fpreserve-invariance ${al_metal_deployment_arg}
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
    almetalspatialscaler.mm
    almetaltemporalscaler.mm
    almetalaurora.mm
    almetaltemporalworldhistory.cpp
    almetalresourceplan.cpp
    almetalresourceplan.h
    almetaldiagnostictelemetry.mm
    almetalavatarresources.cpp
    almetalframe.mm
    almetalframeadmission.cpp
    almetalframecapture.mm
    almetalpbrpreview.cpp
    almetalpipeline.mm
    almetalpipelinecache.mm
    almetalprobecandidates.cpp
    almetalprobeworkload.mm
    almetalresource.mm
    almetalretainedbackground.mm
    almetalscenemonitor.cpp
    almetalshadowcache.cpp
    almetalshadowindirect.mm
    almetalshadowmaterials.mm
    almetalshadowprepare.mm
    almetalshadowvisibility.mm
    almetalstagetiming.mm
    almetalsculptpreview.cpp
    almetalsurface.mm
    almetalterrainpaint.cpp
    almetaltexturebenchmark.mm
    almetaltextureops.cpp
    almetaluirenderer.cpp
    almetalworld.cpp
    almetalshadowtask.cpp
    almetalshadowpublication.cpp
    almetalworlddraws.cpp
    almetalworldanimated.mm
    almetalworldcohort.mm
    almetalworldvisibility.cpp
    almetalworldmaterialdata.mm
    almetalworldresources.mm

    almetalworldinputs.cpp
    almetalworlddiagnostics.cpp
  PUBLIC
    FILE_SET HEADERS
    FILES
      almetaldevice.h
      almetalspatialscaler.h
      almetaltemporalscaler.h
      almetalaurora.h
      almetalauroraconventions.h
      almetaltemporalworldhistory.h
      almetaldiagnostictelemetry.h
      almetalavatarresources.h
      almetalframe.h
      almetalframeadmission.h
      almetalframecapture.h
      almetalquietcapturepolicy.h
      almetalpbrpreview.h
      almetalpipeline.h
      almetalpipelinecache.h
      almetalprobecandidates.h
      almetalprobepublication.h
      almetalprobeworkload.h
      almetalresource.h
      almetalretainedbackground.h
      almetalscenemonitor.h
      almetalshadowcache.h
      almetalshadowindirect.h
      almetalshadowmaterials.h
      almetalstagetiming.h
      almetalshadowcapture.h
      almetalshadowtask.h
      almetalsculptpreview.h
      almetalsurface.h
      almetalterrainpaint.h
      almetaltexturebenchmark.h
      almetaltextureops.h
      almetaluirenderer.h
      almetalworld.h
      almetalworldvisibility.h
      almetalworldmaterialdata.h
      almetalworldresources.h

      almetalworldinputs.h
      almetalworlddraws.h
      almetalworldanimated.h
      almetalworldcohort.h
      almetalworlddiagnostics.h
      almetalshadowprepare.h
      almetalshadowvisibility.h
)
target_compile_definitions(
  almetal
  PRIVATE AL_METAL_BUILTIN_SOURCE_PATH="${AL_METAL_BUILTIN_SOURCE}"
)
set(AL_METAL_SHADOW_VISIBILITY_SHADER
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/almetal_shadow_visibility.metal")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${AL_METAL_SHADOW_VISIBILITY_SHADER}")
file(READ "${AL_METAL_SHADOW_VISIBILITY_SHADER}" AL_METAL_SHADOW_VISIBILITY_SOURCE)
configure_file(almetalshadowvisibilitysource.h.in
               "${CMAKE_CURRENT_BINARY_DIR}/almetalshadowvisibilitysource.h" @ONLY)
target_include_directories(almetal PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(
  almetal
  PRIVATE
    al::flags
    ${AL_METAL_FRAMEWORK}
    ${AL_METALFX_FRAMEWORK}
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
  almetaltemporalscaler.mm
  almetaltemporalworldhistory.cpp
  almetalspatialscaler.mm
  almetalframe.mm
  almetalpipeline.mm
  almetalpipelinecache.mm
  almetalresource.mm
  almetalretainedbackground.mm
  almetalshadowindirect.mm
  almetalshadowmaterials.mm
  almetalshadowprepare.mm
  almetalshadowvisibility.mm
  almetalstagetiming.mm
  almetalsurface.mm
  almetaltexturebenchmark.mm
  almetalworldinputs.cpp
  almetalworldresources.mm
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

if(AL_BUILD_TESTS)
  # Evaluation of the production native Aurora component. Complete retained
  # replay streams are external evidence, so this is not an ordinary CTest.
  add_executable(almetal_aurora_validation
    ../../tools/metal/aurora-native/native_validation.mm
    ../../tools/metal/aurora-evaluation/workload/input_workload.c)
  # Match the retained shared-producer compiler contract; viewer arithmetic
  # flags must not alter the input bytes used by the component comparison.
  set_source_files_properties(
    ../../tools/metal/aurora-evaluation/workload/input_workload.c
    PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
  # Retained workload fingerprints require IEEE producer arithmetic, independently
  # of the viewer's normal CPU optimization flags. This affects only the fixture.
  set_source_files_properties(../../tools/metal/aurora-evaluation/workload/input_workload.c
    PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off")
  set_target_properties(almetal_aurora_validation PROPERTIES
    OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES)
  target_include_directories(almetal_aurora_validation PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_options(almetal_aurora_validation PRIVATE
    "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
  target_link_libraries(almetal_aurora_validation PRIVATE almetal al::flags "-framework Foundation")
  # External complete-sequence reproducer, intentionally not an ordinary CTest:
  # it needs the foliage fixture's exported inputs, including initial warm-up.
  # Link only the native SDK so pool/wrapper/history code cannot affect replay.
  add_executable(almetal_temporal_direct_replay tests/almetal_temporal_direct_replay.mm)
  set_target_properties(almetal_temporal_direct_replay PROPERTIES
    OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED YES)
  target_compile_options(almetal_temporal_direct_replay PRIVATE -fobjc-arc)
  target_link_libraries(almetal_temporal_direct_replay PRIVATE
    al::flags "-framework Foundation" "-framework Metal" "-framework MetalFX")
  # Catch assigns included cases the fragment's file tag. Register those
  # fragments explicitly, while compiling them only through their owning TU.
  set(almetal_native_case_fragments
    tests/almetal_empty_light_pass_native.inl
    tests/almetal_probe_lighting_fusion_test.inl
    tests/almetal_terrain_pass_publication_test.inl
    tests/almetal_world_body_palette_native.inl
    tests/almetal_world_cpu_bound_intervals_test.inl
    tests/almetal_world_published_cpu_visibility_test.inl
    tests/almetal_world_scalar_recipe_test.inl
  )
  set_source_files_properties(${almetal_native_case_fragments} PROPERTIES HEADER_FILE_ONLY TRUE)
  al_add_test_binary(
    almetal_tests
    PROJECT almetal
    SOURCES tests/almetal_ssr_temporal_coordinates_test.mm
            tests/almetal_temporal_water_test.mm
            tests/almetal_aurora_inputs_test.mm
            tests/almetal_temporal_glass_test.mm
            tests/almetal_temporal_visibility_test.mm
            tests/almetal_temporal_foliage_stability_test.mm
            tests/almetal_temporal_presentation_test.mm
            tests/almetal_temporal_orchestration_test.mm
            tests/almetal_temporal_scaler_test.mm
            tests/almetal_temporal_motion_test.mm
            tests/almetal_temporal_history_test.cpp
            tests/almetal_spatial_scaler_test.mm
            tests/almetal_spatial_presentation_test.mm
            tests/almetal_command_timing_test.mm
            tests/almetal_resource_plan_test.cpp
            tests/almetal_resource_plan_native_test.mm

            tests/almetal_surface_resource_native_test.mm
            tests/almetal_retained_background_test.mm
            tests/almetal_scene_monitor_test.mm
            tests/almetal_render_info_test.mm
            tests/almetal_avatar_coverage_test.mm
            tests/almetal_postchain_parity_test.mm
            tests/almetal_exposure_transition_test.mm
            tests/almetal_postchain_legacy_admission_test.mm
            tests/almetal_lens_policy_admission_test.mm
            tests/almetal_linear_screen_boundary_test.mm
            tests/almetal_reflection_policy_test.mm
            tests/almetal_probe_hdr_publication_test.mm
            tests/almetal_pbr_probe_formats_test.mm
            tests/almetal_probe_candidates_test.cpp
            tests/almetal_probe_candidate_publication_test.mm
            tests/almetal_probe_candidate_lighting_test.mm
            tests/almetal_probe_candidate_transparency_test.mm
            ../llrender/tests/alworld_lighting_raster_tiles_metal_test.mm
            tests/almetal_finalize_admission_test.mm
            tests/almetal_ssr_isolation_test.mm
            tests/almetal_reference_format_test.mm
            tests/almetal_cutout_coverage_test.mm
            tests/almetal_smaa_stencil_test.mm
            tests/almetal_cutout_alpha_resample_test.mm
            tests/almetal_water_normal_filter_test.mm
            tests/almetal_water_haze_projection_test.mm
            tests/almetal_hdr_finite_test.mm
            tests/almetal_sky_modernization_test.mm
            tests/almetal_classic_avatar_pose_test.mm
            tests/almetal_avatar_resources_test.mm tests/almetal_gpu_test.mm tests/almetal_encoder_cache_test.mm
            tests/almetal_indexed_join_test.mm
            tests/almetal_completion_test.mm tests/almetal_world_data_test.mm
            tests/almetal_submission_owner_test.mm
            tests/almetal_main_lighting_renderpass_test.mm
            tests/almetal_main_frame_admission_test.mm
            tests/almetal_shadow_recipe_test.mm

            tests/almetal_world_material_source_test.mm
            tests/almetal_world_resources_test.mm
            tests/almetal_world_source_owner_test.mm
            tests/almetal_material_sampling_test.mm
            tests/almetal_world_inputs_test.mm
            tests/almetal_world_draws_test.mm
            tests/almetal_world_cohort_test.mm
            tests/almetal_world_cohort_renderer_test.mm
            tests/almetal_borrowed_target_test.mm
            tests/almetal_base_lighting_fastpath_test.mm
            tests/almetal_zero_lighting_test.mm
            tests/almetal_debug_buffer_test.mm
            tests/almetal_shader_reload_test.mm
            tests/almetal_utility_reload_test.mm
            tests/almetal_preview_reload_test.mm
            tests/almetal_hero_overwrite_test.mm
            tests/almetal_shadow_indirect_test.mm
            tests/almetal_shadow_materials_test.mm
            tests/almetal_stage_timing_test.mm
            tests/almetal_frame_profile_test.mm
            tests/almetal_benchmark_test.mm
            tests/almetal_pipeline_cache_test.mm
            tests/almetal_shadow_cache_test.mm
            tests/almetal_shadow_capture_test.mm
            tests/almetal_shadow_prepare_test.mm
            tests/almetal_shadow_publication_test.mm
            tests/almetal_shadow_encoding_context_test.cpp
            tests/almetal_shadow_task_test.mm
            tests/almetal_shadow_extended_test.mm
            tests/almetal_shadow_generation_benchmark.mm
            tests/almetal_shadow_source_benchmark.mm
            tests/almetal_frame_admission_test.cpp

            tests/almetal_attachment_palette_test.mm
            tests/almetal_attachment_palette_benchmark.mm
            tests/almetal_shadow_gather_test.mm
            tests/almetal_world_diagnostics_test.cpp
            tests/almetal_gbuffer_constants_test.mm
            tests/almetal_probe_workload_test.mm
            tests/almetal_frame_capture_test.mm
            tests/almetal_quiet_capture_policy_test.cpp
            tests/almetal_diagnostic_telemetry_test.mm
            tests/almetal_function_constants_test.mm

            tests/almetal_world_library_test.mm
            ${almetal_native_case_fragments}

    LIBRARIES almetal llcommon llinventory
    # Correctness cases exercise the selected production consumer policy.
    RESOURCE_LOCK apple_gpu
  )
  # This TU owns fixtures and included cases only; it has no direct file-tagged
  # cases and must not create an empty per-file CTest entry.
  target_sources(almetal_tests PRIVATE tests/almetal_world_visibility_test.mm)
  # External-input research fixtures are explicitly manual, never an empty default CTest suite.
  # Invoke the hidden .probe-accumulator-* tags with the four documented source/library inputs.
  target_sources(almetal_tests PRIVATE tests/almetal_probe_accumulator_test.mm)
  set_source_files_properties(tests/almetal_command_timing_test.mm tests/almetal_avatar_resources_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_resource_plan_test.cpp tests/almetal_resource_plan_native_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_surface_resource_native_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_temporal_visibility_test.mm
                              tests/almetal_ssr_temporal_coordinates_test.mm
                              tests/almetal_temporal_water_test.mm
                              tests/almetal_temporal_glass_test.mm
                              tests/almetal_aurora_inputs_test.mm
                              tests/almetal_temporal_foliage_stability_test.mm
                              tests/almetal_temporal_presentation_test.mm
                              tests/almetal_temporal_orchestration_test.mm
                              tests/almetal_temporal_scaler_test.mm
                              tests/almetal_temporal_motion_test.mm
                              tests/almetal_temporal_history_test.cpp
                              tests/almetal_spatial_scaler_test.mm
                              tests/almetal_spatial_presentation_test.mm
                              PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_retained_background_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_scene_monitor_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_render_info_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_avatar_coverage_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_postchain_parity_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_exposure_transition_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_postchain_legacy_admission_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_lens_policy_admission_test.mm
                              tests/almetal_linear_screen_boundary_test.mm
                              tests/almetal_reflection_policy_test.mm
                              tests/almetal_probe_hdr_publication_test.mm
                              tests/almetal_pbr_probe_formats_test.mm
                              tests/almetal_finalize_admission_test.mm
                              tests/almetal_ssr_isolation_test.mm
                              tests/almetal_reference_format_test.mm
                              tests/almetal_cutout_coverage_test.mm
                              tests/almetal_smaa_stencil_test.mm
                              tests/almetal_cutout_alpha_resample_test.mm
                              tests/almetal_water_normal_filter_test.mm
                              tests/almetal_water_haze_projection_test.mm
                              PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_hdr_finite_test.mm PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_DEFINITIONS "AL_METAL_HDR_SOURCE_PATH=\"${AL_METAL_BUILTIN_SOURCE}\"")
  set_source_files_properties(tests/almetal_sky_modernization_test.mm PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_classic_avatar_pose_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_gpu_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_encoder_cache_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_indexed_join_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_completion_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_world_data_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_submission_owner_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_material_sampling_test.mm tests/almetal_world_inputs_test.mm tests/almetal_world_draws_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_cache_test.mm
                              tests/almetal_shadow_prepare_test.mm
                              tests/almetal_shadow_publication_test.mm
                              tests/almetal_shadow_encoding_context_test.cpp
                              tests/almetal_shadow_task_test.mm
                              tests/almetal_shadow_extended_test.mm
                              tests/almetal_shadow_generation_benchmark.mm
                              tests/almetal_shadow_source_benchmark.mm
                              tests/almetal_frame_admission_test.cpp

                              PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_attachment_palette_test.mm
                              tests/almetal_attachment_palette_benchmark.mm
                              PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_borrowed_target_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_base_lighting_fastpath_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_zero_lighting_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_debug_buffer_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shader_reload_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_utility_reload_test.mm tests/almetal_preview_reload_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_hero_overwrite_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_indirect_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_materials_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_stage_timing_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_frame_profile_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_benchmark_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_pipeline_cache_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_capture_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_shadow_gather_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_gbuffer_constants_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_probe_workload_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(../llrender/tests/alworld_lighting_raster_tiles_metal_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_frame_capture_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_diagnostic_telemetry_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  set_source_files_properties(tests/almetal_function_constants_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  if(AL_METAL_BUILTIN_LIBRARY)
    # Compare actual production vertex outputs across coplanar rigged passes.
    # This tiny standalone fixture consumes the exact offline viewer library.
    add_executable(almetal_rigged_depth_invariance_tests
      tests/almetal_rigged_depth_invariance_test.mm)
    set_target_properties(almetal_rigged_depth_invariance_tests PROPERTIES
      OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED YES)
    target_compile_options(almetal_rigged_depth_invariance_tests PRIVATE -fobjc-arc)
    target_link_libraries(almetal_rigged_depth_invariance_tests PRIVATE
      al::flags ${AL_METAL_FRAMEWORK} "-framework Foundation")
    add_dependencies(almetal_rigged_depth_invariance_tests
      ${AL_METAL_BUILTIN_LIBRARY_TARGET})
    add_test(NAME almetal_rigged_depth_invariance
      COMMAND almetal_rigged_depth_invariance_tests "${AL_METAL_BUILTIN_LIBRARY}")
    set_tests_properties(almetal_rigged_depth_invariance PROPERTIES
      RESOURCE_LOCK apple_gpu)
    # Exercise the same offline asset lookup as the viewer. CTest executes
    # TARGET_FILE directly; this bundle creates no window or app launch.
    set_target_properties(almetal_tests PROPERTIES MACOSX_BUNDLE TRUE)
    target_sources(almetal_tests PRIVATE "${AL_METAL_BUILTIN_LIBRARY}")
    # Multi-config source properties belong to concrete generated paths.
    # Resource rules then also copy shader-only updates without relinking.
    if(CMAKE_CONFIGURATION_TYPES)
      set(almetal_test_configs ${CMAKE_CONFIGURATION_TYPES})
    else()
      set(almetal_test_configs "${CMAKE_BUILD_TYPE}")
    endif()
    foreach(almetal_test_config IN LISTS almetal_test_configs)
      string(REPLACE "$<CONFIG>" "${almetal_test_config}"
             almetal_test_asset "${AL_METAL_BUILTIN_LIBRARY}")
      set_source_files_properties("${almetal_test_asset}" PROPERTIES
        GENERATED TRUE MACOSX_PACKAGE_LOCATION Resources)
    endforeach()
    target_compile_definitions(
      almetal_tests
      PRIVATE AL_METAL_BUILTIN_LIBRARY_PATH="${AL_METAL_BUILTIN_LIBRARY}"
    )
  endif()
endif()

set_source_files_properties(tests/almetal_world_visibility_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(tests/almetal_main_lighting_renderpass_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(tests/almetal_main_frame_admission_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(tests/almetal_shadow_recipe_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

set_source_files_properties(tests/almetal_world_source_owner_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

set_source_files_properties(tests/almetal_world_material_source_test.mm almetalworldmaterialdata.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

set_source_files_properties(tests/almetal_world_resources_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(tests/almetal_world_library_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(tests/almetal_world_cohort_test.mm
  tests/almetal_world_cohort_renderer_test.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(almetalworldanimated.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
