#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Callback for frame data from OnPaint.
using FrameCallback = std::function<void(int32_t buffer_id, const void *pixels,
                                         int width, int height)>;

// Callback for buffer state changes.
using StateCallback = std::function<void(int32_t buffer_id)>;

class LuaRuntime; // forward declaration

class Engine {
public:
  Engine();
  ~Engine();

  bool initialize(int argc, char *argv[]);
  int child_exit_code() const;
  void do_message_loop_work();
  void shutdown();

  // --- Buffer management ---
  int32_t create_buffer(const std::string &url,
                        uint32_t viewport_width, uint32_t viewport_height);
  void close_buffer(int32_t buffer_id);
  size_t buffer_count() const;

  // --- Navigation ---
  void navigate(int32_t buffer_id, const std::string &url);
  void go_back(int32_t buffer_id);
  void go_forward(int32_t buffer_id);
  void reload(int32_t buffer_id);
  void stop_load(int32_t buffer_id);

  // --- Queries ---
  std::string get_title(int32_t buffer_id);
  std::string get_url(int32_t buffer_id);
  bool is_loading(int32_t buffer_id);
  bool can_go_back(int32_t buffer_id);
  bool can_go_forward(int32_t buffer_id);
  double load_progress(int32_t buffer_id);
  std::string frame_shm_name(int32_t buffer_id) const;

  // --- Active buffer ---
  int32_t active_buffer_id() const;
  void set_active_buffer(int32_t buffer_id);
  std::vector<int32_t> list_buffer_ids() const;

  // --- Viewport ---
  void resize(int32_t buffer_id, int width, int height);

  // --- Callbacks ---
  void set_frame_callback(FrameCallback cb);
  void set_state_callback(StateCallback cb);

  // --- Lua ---
  LuaRuntime *lua_runtime();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
