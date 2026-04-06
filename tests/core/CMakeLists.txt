# Unit tests for dirtferret-core
# These do NOT link against CEF — only our own code and Cap'n Proto.

find_package(GTest REQUIRED)

# --- test-schema: Cap'n Proto schema round-trip tests ---
add_executable(test-schema test_schema.cc)
target_include_directories(test-schema PRIVATE
  ${CMAKE_SOURCE_DIR}/core
)
target_link_libraries(test-schema PRIVATE
  dirtferret-schema
  GTest::gtest_main
)
add_test(NAME test-schema COMMAND test-schema)

# --- test-shm: shared memory frame pool tests ---
add_executable(
  test-shm
  test_shm.cc
  ${CMAKE_SOURCE_DIR}/core/shm/frame_pool.cc
)
target_include_directories(test-shm PRIVATE
  ${CMAKE_SOURCE_DIR}/core
)
target_link_libraries(test-shm PRIVATE
  GTest::gtest_main
  rt
)
add_test(NAME test-shm COMMAND test-shm)

# # --- test-rpc: RPC server fundamentals (no CEF, no capnp runtime) ---
# add_executable(
#   test-rpc
#   test_rpc.cc
#   ${CMAKE_SOURCE_DIR}/core/rpc/rpc_server.cc
# )
# target_include_directories(test-rpc PRIVATE
#   ${CMAKE_SOURCE_DIR}/core
# )
# target_link_libraries(test-rpc PRIVATE
#   GTest::gtest_main
# )
# add_test(NAME test-rpc COMMAND test-rpc)

# --- test-input: terminal-to-CEF input translation tests ---
add_executable(test-input test_input.cc)
target_include_directories(test-input PRIVATE
  ${CMAKE_SOURCE_DIR}/core
)
target_link_libraries(test-input PRIVATE
  GTest::gtest_main
)
add_test(NAME test-input COMMAND test-input)
