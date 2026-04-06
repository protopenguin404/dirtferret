#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Callback for frame data from OnPaint.
// (buffer_id, pixel_data, width, height, dirty_rects)
using FrameCallback = std::function<void(int32_t buffer_id, const void *pixels,
                                         int width, int height)>;

// The Engine is the core's interface to CEF.
// It manages buffer (browser) lifecycles and provides methods
// for navigation, state queries, and input injection.
//
// The RPC server calls these methods. The Engine posts CEF work
// to TID_UI as needed. This class is NOT CefBase-derived.
//
// CEF headers are confined to engine.cc — consumers of this header
// (e.g. RPC server, main.cc) do not need -fno-exceptions.
class Engine {
public:
  Engine();
  ~Engine();

  // Initialize CEF. Returns false if this is a child process
  // (caller should exit with child_exit_code()) or if init fails.
  bool initialize(int argc, char *argv[]);

  // If initialize() returned false, the exit code for a child process.
  int child_exit_code() const;

  // Run one iteration of CEF's message loop.
  void do_message_loop_work();

  // Shut down CEF cleanly.
  void shutdown();

  // --- Buffer management ---
  // NOTE: These must be called from the CEF UI thread (TID_UI).
  // The RPC server is responsible for posting to TID_UI.

  struct BufOptions {};
  int32_t create_buffer(int stub); // Stub
  void navigate(int32_t buffer_id, const std::string &url);
  void go_back(int32_t buffer_id);
  void go_forward(int32_t buffer_id);
  void reload(int32_t buffer_id);
  void stop_load(int32_t buffer_id);

  // --- Queries ---
  std::string get_title(int32_t buffer_id);
  std::string get_url(int32_t buffer_id);
  // BufferInfo get_buffer_info(int32_t buffer_id);

  // --- Input injection ---
  void resize(int32_t buffer_id, int width, int height);

  // --- Frame delivery ---
  void set_frame_callback(FrameCallback cb);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
