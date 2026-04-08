cmake_minimum_required(VERSION 3.21)
project(dirtferret)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- CEF setup ---
if(NOT CEF_ROOT AND DEFINED ENV{CEF_ROOT})
  set(CEF_ROOT "$ENV{CEF_ROOT}")
endif()
if(NOT CEF_ROOT)
  message(FATAL_ERROR "CEF_ROOT must be set (cmake -DCEF_ROOT=... or export CEF_ROOT=...)")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# Build libcef_dll_wrapper from the CEF distribution (read-only nix store is fine)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} ${CMAKE_BINARY_DIR}/libcef_dll_wrapper)

# --- Lua setup ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED luajit)

# --- Cap'n Proto setup ---
find_package(CapnProto REQUIRED)

# Generate C++ code from .capnp schemas
set(SCHEMA_DIR "${CMAKE_SOURCE_DIR}/schema")
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS
  ${SCHEMA_DIR}/types.capnp
  ${SCHEMA_DIR}/core.capnp
)

# Schema library — both core and tests link against this
add_library(dirtferret-schema STATIC ${CAPNP_SRCS})
target_include_directories(dirtferret-schema PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(dirtferret-schema PUBLIC CapnProto::capnp CapnProto::capnp-rpc)

# --- Core ---
add_subdirectory(core)

# --- Tests ---
enable_testing()
add_subdirectory(tests/core)

# Print config
message(STATUS "CEF_ROOT: ${CEF_ROOT}")
message(STATUS "Cap'n Proto: ${CAPNP_EXECUTABLE}")
