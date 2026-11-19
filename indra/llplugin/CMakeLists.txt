# -*- cmake -*-

add_library(llplugin STATIC)

target_sources(
  llplugin
  PRIVATE
    llpluginclassmedia.cpp
    llplugininstance.cpp
    llpluginmessage.cpp
    llpluginmessagepipe.cpp
    llpluginprocesschild.cpp
    llpluginprocessparent.cpp
    llpluginsharedmemory.cpp
  PUBLIC
    FILE_SET HEADERS
      FILES
        llpluginclassmedia.h
        llpluginclassmediaowner.h
        llplugininstance.h
        llpluginmessage.h
        llpluginmessageclasses.h
        llpluginmessagepipe.h
        llpluginprocesschild.h
        llpluginprocessparent.h
        llpluginsharedmemory.h
)

target_link_libraries(llplugin PRIVATE al::flags PUBLIC llcommon llmath llmessage llxml)
target_precompile_headers(llplugin REUSE_FROM llprecompiled)

if(AL_BUILD_TESTS AND DARWIN)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(shutdown_handshake_source "${CMAKE_CURRENT_BINARY_DIR}/llplugin_shutdown_handshake_generated.cpp")
  add_custom_command(OUTPUT "${shutdown_handshake_source}"
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/llplugin_shutdown_handshake_test.py"
      --source-root "${CMAKE_CURRENT_SOURCE_DIR}/../.."
      --emit-source "${shutdown_handshake_source}"
    DEPENDS tests/llplugin_shutdown_handshake_test.py tests/llplugin_shutdown_handshake_fixture.cpp
      llpluginprocessparent.cpp llpluginprocessparent.h llpluginprocesschild.cpp llpluginprocesschild.h
    VERBATIM)
  add_executable(llplugin_shutdown_handshake_contract_test "${shutdown_handshake_source}")
  target_link_libraries(llplugin_shutdown_handshake_contract_test PRIVATE al::flags)
  target_compile_features(llplugin_shutdown_handshake_contract_test PRIVATE cxx_std_20)
  target_compile_options(llplugin_shutdown_handshake_contract_test PRIVATE
    -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  target_link_options(llplugin_shutdown_handshake_contract_test PRIVATE -fsanitize=address,undefined)
  add_test(NAME "llplugin/shutdown_handshake_contract" COMMAND llplugin_shutdown_handshake_contract_test)
  set_tests_properties("llplugin/shutdown_handshake_contract" PROPERTIES TIMEOUT 60 LABELS "llplugin")
endif()
