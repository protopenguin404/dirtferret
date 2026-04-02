# CEF terminal browser — two targets: backend (CEF daemon) and frontend (terminal client).

# Determine the target output directory.
SET_CEF_TARGET_OUT_DIR()

# --- Shared sources (API layer + IPC, used by both targets) ---
set(SHARED_SRCS
  api/dispatcher.cc
  ipc/serialization.cc
  ipc/unix_socket_transport.cc
  )

# --- Backend (CEF daemon + API + plugins) ---

# Find Lua for the plugin runtime.
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED lua)

set(BACKEND_SRCS
  backend_main.cc
  ${SHARED_SRCS}
  plugin/lua_host.cc
  backend/app.cc
  backend/app.h
  backend/client.cc
  backend/client.h
  backend/engine.cc
  backend/engine.h
  backend/minimal_renderer.cc
  backend/minimal_renderer.h
  )

add_executable(cef-backend ${BACKEND_SRCS})
SET_EXECUTABLE_TARGET_PROPERTIES(cef-backend)

# Backend needs src/ as an include root so headers resolve as "api/types.h" etc.
target_include_directories(cef-backend PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LUA_INCLUDE_DIRS}
  )

# Find X11 — required by CEF even in offscreen mode.
find_package(X11 QUIET)
if(X11_FOUND)
  set(_X11_LIBS ${X11_LIBRARIES})
else()
  execute_process(
    COMMAND ldd "${CEF_LIB_RELEASE}"
    COMMAND grep libX11
    OUTPUT_VARIABLE _x11_ldd
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(REGEX MATCH "=> ([^ ]+)" _x11_match "${_x11_ldd}")
  set(_X11_LIBS "${CMAKE_MATCH_1}")
endif()
message(STATUS "Using X11: ${_X11_LIBS}")

target_link_libraries(cef-backend libcef_dll_wrapper ${_X11_LIBS} ${LUA_LIBRARIES})

ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
target_link_libraries(cef-backend libcef_lib)

# Copy CEF binary and resource files to the target output directory.
COPY_FILES("cef-backend" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("cef-backend" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

# --- Frontend (terminal client — NO CEF dependency) ---
set(FRONTEND_SRCS
  frontend_main.cc
  ${SHARED_SRCS}
  # frontend/kitty_renderer.cc — TODO: kitty stuff
  # frontend/input.cc          — TODO: terminal input implementation
  )

add_executable(cef-frontend ${FRONTEND_SRCS})

# Frontend also needs src/ as include root.
target_include_directories(cef-frontend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Frontend does NOT link against CEF. This is intentional —
# it enforces the process boundary at the build level.

# --- Tests (Google Test) ---
find_package(GTest REQUIRED)
enable_testing()

# Test target: IPC serialization (existing code — should pass now).
add_executable(test-serialization
  tests/test_serialization.cc
  ${SHARED_SRCS}
  )
target_include_directories(test-serialization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test-serialization GTest::gtest GTest::gtest_main)
add_test(NAME serialization COMMAND test-serialization)

# Test target: FRAME wire format (needs transport extension).
# Won't pass until you extend send()/try_parse_message() for FRAME metadata.
add_executable(test-frame-wire
  tests/test_frame_wire.cc
  ${SHARED_SRCS}
  )
target_include_directories(test-frame-wire PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test-frame-wire GTest::gtest GTest::gtest_main)
add_test(NAME frame-wire COMMAND test-frame-wire)

# Test target: frontend (Terminal + KittyRenderer).
# Won't compile until you implement both classes.
add_executable(test-frontend
  tests/test_terminal.cc
  tests/test_kitty_renderer.cc
  frontend/terminal.cc
  frontend/kitty_renderer.cc
  ${SHARED_SRCS}
  )
target_include_directories(test-frontend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test-frontend GTest::gtest GTest::gtest_main)
add_test(NAME frontend COMMAND test-frontend)
