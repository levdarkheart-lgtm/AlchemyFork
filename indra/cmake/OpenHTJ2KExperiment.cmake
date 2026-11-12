# Pinned CPU decoder experiment. A CI build must carry a matching build receipt;
# the original local experiment keeps its exact prebuilt-archive guard.
option(AL_EXPERIMENTAL_OPENHTJ2K "Use the local OpenHTJ2K texture decoder experiment" OFF)
if(NOT AL_EXPERIMENTAL_OPENHTJ2K)
  return()
endif()
if(AL_USE_KDU)
  message(FATAL_ERROR "OpenHTJ2K experiment requires the open-source JPEG2000 implementation")
endif()

set(AL_OPENHTJ2K_SOURCE_DIR "" CACHE PATH "Pinned OpenHTJ2K source checkout")
set(AL_OPENHTJ2K_LIBRARY "" CACHE FILEPATH "Pinned ARM64 Release OpenHTJ2K static archive")
set(AL_OPENHTJ2K_BUILD_RECEIPT "" CACHE FILEPATH "Receipt from tools/metal/build_openhtj2k.sh")
foreach(path AL_OPENHTJ2K_SOURCE_DIR AL_OPENHTJ2K_LIBRARY)
  if(NOT IS_ABSOLUTE "${${path}}" OR NOT EXISTS "${${path}}")
    message(FATAL_ERROR "${path} must point to the explicit local experiment dependency")
  endif()
endforeach()
execute_process(COMMAND git -C "${AL_OPENHTJ2K_SOURCE_DIR}" rev-parse HEAD
  RESULT_VARIABLE openht_git_result OUTPUT_VARIABLE openht_revision OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT openht_git_result EQUAL 0 OR NOT openht_revision STREQUAL "72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2")
  message(FATAL_ERROR "OpenHTJ2K experiment requires the benchmarked 0.27.0 source revision")
endif()
execute_process(COMMAND git -C "${AL_OPENHTJ2K_SOURCE_DIR}" status --porcelain --untracked-files=all
  RESULT_VARIABLE openht_status_result OUTPUT_VARIABLE openht_status OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT openht_status_result EQUAL 0 OR NOT openht_status STREQUAL "")
  message(FATAL_ERROR "OpenHTJ2K source checkout must be entirely clean at the pinned revision")
endif()
file(SHA256 "${AL_OPENHTJ2K_LIBRARY}" openht_archive_sha256)
if(AL_OPENHTJ2K_BUILD_RECEIPT)
  if(NOT IS_ABSOLUTE "${AL_OPENHTJ2K_BUILD_RECEIPT}" OR NOT EXISTS "${AL_OPENHTJ2K_BUILD_RECEIPT}")
    message(FATAL_ERROR "AL_OPENHTJ2K_BUILD_RECEIPT must name the existing absolute build receipt")
  endif()
  file(READ "${AL_OPENHTJ2K_BUILD_RECEIPT}" openht_receipt)
  foreach(field schema_version source_revision source_clean architecture configuration deployment_target source_dir library_path library_sha256)
    string(JSON openht_receipt_${field} ERROR_VARIABLE openht_receipt_error GET "${openht_receipt}" "${field}")
    if(openht_receipt_error)
      message(FATAL_ERROR "OpenHTJ2K build receipt has an invalid or missing ${field}")
    endif()
  endforeach()
  string(JSON openht_clean_type TYPE "${openht_receipt}" source_clean)
  file(REAL_PATH "${AL_OPENHTJ2K_SOURCE_DIR}" openht_source_real)
  file(REAL_PATH "${AL_OPENHTJ2K_LIBRARY}" openht_library_real)
  if(NOT openht_receipt_schema_version STREQUAL "1" OR
      NOT openht_receipt_source_revision STREQUAL openht_revision OR
      NOT openht_clean_type STREQUAL "BOOLEAN" OR NOT openht_receipt_source_clean OR
      NOT openht_receipt_architecture STREQUAL "arm64" OR
      NOT openht_receipt_configuration STREQUAL "Release" OR
      NOT openht_receipt_deployment_target STREQUAL "26.0" OR
      NOT openht_receipt_source_dir STREQUAL openht_source_real OR
      NOT openht_receipt_library_path STREQUAL openht_library_real OR
      NOT openht_receipt_library_sha256 STREQUAL openht_archive_sha256)
    message(FATAL_ERROR "OpenHTJ2K build receipt does not match the pinned clean ARM64 Release source/archive")
  endif()
elseif(NOT openht_archive_sha256 STREQUAL "43190158ea345322dcdc89175d5ec36e8212933fdf83236b3f8fb668dd225182")
  message(FATAL_ERROR "OpenHTJ2K experiment archive differs from the benchmarked ARM64 Release archive; rebuilt archives require AL_OPENHTJ2K_BUILD_RECEIPT")
endif()
file(READ "${AL_OPENHTJ2K_LIBRARY}" openht_archive_magic OFFSET 0 LIMIT 8 HEX)
execute_process(COMMAND xcrun lipo -archs "${AL_OPENHTJ2K_LIBRARY}"
  RESULT_VARIABLE openht_arch_result OUTPUT_VARIABLE openht_arches OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT openht_archive_magic STREQUAL "213c617263683e0a" OR
    NOT openht_arch_result EQUAL 0 OR NOT openht_arches STREQUAL "arm64")
  message(FATAL_ERROR "OpenHTJ2K must be a static archive containing only native arm64 objects")
endif()
find_package(Threads REQUIRED)
add_library(ll::openhtj2k STATIC IMPORTED GLOBAL)
set_target_properties(ll::openhtj2k PROPERTIES
  IMPORTED_LOCATION "${AL_OPENHTJ2K_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${AL_OPENHTJ2K_SOURCE_DIR}/source/core/interface;${AL_OPENHTJ2K_SOURCE_DIR}/source/core/common"
  INTERFACE_COMPILE_DEFINITIONS OHTJ2K_STATIC
  INTERFACE_LINK_LIBRARIES Threads::Threads)
message(STATUS "Experimental JPEG2000 decode: OpenHTJ2K 0.27.0 (${openht_revision}); encoder/header: OpenJPEG")
