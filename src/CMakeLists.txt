# Minimal CEF terminal browser target.

# Determine the target output directory.
SET_CEF_TARGET_OUT_DIR()

# Source files.
set(SRCS
  main.cc
  )

# Executable target.
add_executable(cef-terminal ${SRCS})
SET_EXECUTABLE_TARGET_PROPERTIES(cef-terminal)
# Find X11 — available in devshell via libx11 in buildInputs.
# Falls back to extracting from libcef.so's ldd output on NixOS.
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

target_link_libraries(cef-terminal libcef_dll_wrapper ${_X11_LIBS})

# Add the CEF binary as an imported library.
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
target_link_libraries(cef-terminal libcef_lib)

# Copy CEF binary and resource files to the target output directory.
COPY_FILES("cef-terminal" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("cef-terminal" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
