# Deliberately local, pinned CPU decoder experiment. Not a shipping dependency.
option(AL_EXPERIMENTAL_OPENHTJ2K "Use the local OpenHTJ2K texture decoder experiment" OFF)
if(NOT AL_EXPERIMENTAL_OPENHTJ2K)
  return()
endif()
if(AL_USE_KDU)
  message(FATAL_ERROR "OpenHTJ2K experiment requires the open-source JPEG2000 implementation")
endif()

set(AL_OPENHTJ2K_SOURCE_DIR "" CACHE PATH "Pinned OpenHTJ2K source checkout")
set(AL_OPENHTJ2K_LIBRARY "" CACHE FILEPATH "Pinned ARM64 Release OpenHTJ2K static archive")
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
execute_process(COMMAND git -C "${AL_OPENHTJ2K_SOURCE_DIR}" diff --quiet HEAD -- source/core/interface source/core/common
  RESULT_VARIABLE openht_headers_dirty)
if(NOT openht_headers_dirty EQUAL 0)
  message(FATAL_ERROR "OpenHTJ2K experiment headers differ from the pinned archive's source revision")
endif()
file(SHA256 "${AL_OPENHTJ2K_LIBRARY}" openht_archive_sha256)
if(NOT openht_archive_sha256 STREQUAL "43190158ea345322dcdc89175d5ec36e8212933fdf83236b3f8fb668dd225182")
  message(FATAL_ERROR "OpenHTJ2K experiment archive differs from the benchmarked ARM64 Release archive")
endif()
find_package(Threads REQUIRED)
add_library(ll::openhtj2k STATIC IMPORTED GLOBAL)
set_target_properties(ll::openhtj2k PROPERTIES
  IMPORTED_LOCATION "${AL_OPENHTJ2K_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${AL_OPENHTJ2K_SOURCE_DIR}/source/core/interface;${AL_OPENHTJ2K_SOURCE_DIR}/source/core/common"
  INTERFACE_COMPILE_DEFINITIONS OHTJ2K_STATIC
  INTERFACE_LINK_LIBRARIES Threads::Threads)
message(STATUS "Experimental JPEG2000 decode: OpenHTJ2K 0.27.0 (${openht_revision}); encoder/header: OpenJPEG")
