# dirtferret-core: the browser kernel

set(CORE_SOURCES
  main.cc

  engine/app.cc
  engine/client.cc
  engine/renderer.cc
  engine/engine.cc

  shm/frame_pool.cc
)

add_executable(dirtferret-core ${CORE_SOURCES})

target_include_directories(dirtferret-core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}       # for "engine/app.h", "shm/frame_pool.h"
  ${CEF_ROOT}                       # for "include/cef_app.h"
)

target_link_libraries(dirtferret-core PRIVATE
  libcef_dll_wrapper
  ${CEF_LIB_RELEASE}
  dirtferret-schema
  rt                                # for shm_open / shm_unlink
  pthread
)

# CEF compile flags (no exceptions, etc.)
SET_EXECUTABLE_TARGET_PROPERTIES(dirtferret-core)
