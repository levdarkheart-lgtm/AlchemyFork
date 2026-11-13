# -*- cmake -*-

# Construct the viewer version number based on the indra/VIEWER_VERSION file
if(NOT DEFINED VIEWER_SHORT_VERSION) # will be true in indra/, false in indra/newview/
  set(VIEWER_VERSION_BASE_FILE "${INDRA_SOURCE_DIR}/newview/VIEWER_VERSION.txt")

  if(EXISTS ${VIEWER_VERSION_BASE_FILE})
    file(STRINGS ${VIEWER_VERSION_BASE_FILE} VIEWER_SHORT_VERSION REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+")
    string(
      REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+"
      "\\1"
      VIEWER_VERSION_MAJOR
      ${VIEWER_SHORT_VERSION}
    )
    string(
      REGEX REPLACE "^[0-9]+\\.([0-9]+)\\.[0-9]+"
      "\\1"
      VIEWER_VERSION_MINOR
      ${VIEWER_SHORT_VERSION}
    )
    string(
      REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)"
      "\\1"
      VIEWER_VERSION_PATCH
      ${VIEWER_SHORT_VERSION}
    )

    # Source identity is independent of the numeric version override used by
    # CI. Always read the current checkout; a previous configure's cached SHA
    # must never describe a different source tree or hide a failed Git probe.
    find_package(Git)
    set(git_head "")
    if(Git_FOUND)
      execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${INDRA_SOURCE_DIR}
        OUTPUT_VARIABLE git_head
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE git_head_result
        ERROR_QUIET
      )
      string(LENGTH "${git_head}" git_head_length)
      if(NOT git_head_result EQUAL 0 OR NOT git_head MATCHES "^[0-9a-f]+$" OR NOT git_head_length EQUAL 40)
        set(git_head "")
      endif()
    endif()

    if(git_head)
      # Keep the history count against the commit it was taken for. The files
      # Git moves HEAD through trigger reconfiguration after a source change.
      execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --git-path HEAD --git-path logs/HEAD
        WORKING_DIRECTORY ${INDRA_SOURCE_DIR}
        OUTPUT_VARIABLE git_head_files
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      string(REPLACE "\n" ";" git_head_files "${git_head_files}")
      foreach(head_file IN LISTS git_head_files)
        if(EXISTS "${INDRA_SOURCE_DIR}/${head_file}")
          set_property(
            DIRECTORY
            APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS "${INDRA_SOURCE_DIR}/${head_file}"
          )
        elseif(EXISTS "${head_file}")
          set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${head_file}")
        endif()
      endforeach()

      if(NOT AL_GIT_REVISION_COMMIT STREQUAL git_head OR NOT DEFINED AL_GIT_REVISION_COUNT)
        execute_process(
          COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
          WORKING_DIRECTORY ${INDRA_SOURCE_DIR}
          OUTPUT_VARIABLE git_count
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(
          AL_GIT_REVISION_COUNT
          "${git_count}"
          CACHE INTERNAL
          "Number of commits reachable from AL_GIT_REVISION_COMMIT"
        )
      endif()
    endif()
    set(
      AL_GIT_REVISION_COMMIT
      "${git_head}"
      CACHE INTERNAL
      "The current source commit and the commit AL_GIT_REVISION_COUNT was taken for"
    )

    # VIEWER_VERSION_REVISION_SOURCE names where the number came from;
    # the configuration report prints it beside the version.
    if(DEFINED ENV{revision})
      set(VIEWER_VERSION_REVISION $ENV{revision})
      set(VIEWER_VERSION_REVISION_SOURCE "environment")
    elseif(DEFINED ENV{GITHUB_RUN_ID})
      set(VIEWER_VERSION_REVISION $ENV{GITHUB_RUN_ID})
      set(VIEWER_VERSION_REVISION_SOURCE "GitHub run id")
    elseif(git_head)
      set(VIEWER_VERSION_REVISION ${AL_GIT_REVISION_COUNT})
      set(VIEWER_VERSION_REVISION_SOURCE "git")
    elseif(Git_FOUND)
      set(VIEWER_VERSION_REVISION 0)
      set(VIEWER_VERSION_REVISION_SOURCE "none: not a git repository")
    else()
      set(VIEWER_VERSION_REVISION 0)
      set(VIEWER_VERSION_REVISION_SOURCE "none: git not found")
    endif()
  else()
    message(SEND_ERROR "Cannot get viewer version from '${VIEWER_VERSION_BASE_FILE}'")
  endif()

  if("${VIEWER_VERSION_REVISION}" STREQUAL "")
    set(VIEWER_VERSION_REVISION 0)
    set(VIEWER_VERSION_REVISION_SOURCE "none: revision was blank")
  endif()
endif()
