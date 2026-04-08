# dirtferret-core: the browser kernel

# --- Engine library (CEF code) — compiled with -fno-exceptions ---
set(ENGINE_SOURCES
  engine/app.cc
  engine/client.cc
  engine/renderer.cc
  engine/engine.cc
  engine/input.cc
  shm/frame_pool.cc
  lua/runtime.cc
)

add_library(dirtferret-engine STATIC ${ENGINE_SOURCES})

target_include_directories(dirtferret-engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}       # for "engine/app.h", "shm/frame_pool.h"
  ${CEF_ROOT}                       # for "include/cef_app.h"
  ${LUAJIT_INCLUDE_DIRS}            # for <lua.h>, <lauxlib.h>, <lualib.h>
)

target_link_libraries(dirtferret-engine PUBLIC
  libcef_dll_wrapper
  ${CEF_LIB_RELEASE}
  ${LUAJIT_LIBRARIES}               # LuaJIT
  rt                                # for shm_open / shm_unlink
  pthread
)

# CEF requires -fno-exceptions — apply ONLY to engine code
SET_LIBRARY_TARGET_PROPERTIES(dirtferret-engine)

# --- Core executable (RPC + main loop) — exceptions enabled ---
add_executable(dirtferret-core main.cc)

target_include_directories(dirtferret-core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(dirtferret-core PRIVATE
  dirtferret-engine
  dirtferret-schema
)
