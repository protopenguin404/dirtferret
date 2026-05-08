#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "engine/region.h"  // Point, Region, RegionSet, ElementInfo, DirtyRect

// Callback for buffer state changes.
using StateCallback = std::function<void(int32_t buffer_id)>;

// Callback for buffer lifecycle events.
using BufferCreatedCallback = std::function<void(int32_t buffer_id)>;
using BufferClosedCallback = std::function<void(int32_t buffer_id)>;

class DomBridge;   // forward declaration
class LuaRuntime;  // forward declaration

// Testable free function: find the nearest element in a direction.
// Returns index into elements, or -1 if none found in the given direction.
int find_nearest_in_direction(const std::vector<ElementInfo>& elements,
                              int current_index, int dx, int dy);

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

  // --- Input injection ---
  void send_key_event(int32_t buffer_id, uint32_t key_type,
                      uint32_t key_code, uint32_t character,
                      uint32_t modifiers);
  void send_mouse_event(int32_t buffer_id, uint32_t event_type,
                        int x, int y, uint32_t button, uint32_t modifiers);
  void send_scroll_event(int32_t buffer_id, int delta_x, int delta_y);

  // --- Callbacks (non-frame events, delivered via RPC) ---
  void set_state_callback(StateCallback cb);
  void set_buffer_created_callback(BufferCreatedCallback cb);
  void set_buffer_closed_callback(BufferClosedCallback cb);

  // --- DOM queries (async, callback-based) ---
  void element_at(int32_t buffer_id, int x, int y,
                  std::function<void(ElementInfo)> callback);
  void query(int32_t buffer_id, const std::string& selector,
             std::function<void(std::vector<ElementInfo>)> callback);

  // --- Region management ---
  uint32_t region_add(int32_t buffer_id, int x, int y);
  void region_remove(int32_t buffer_id, uint32_t region_id);
  void region_move(int32_t buffer_id, uint32_t region_id, int x, int y);
  void region_select(int32_t buffer_id, Scope scope,
                     const std::string& selector_arg,
                     std::function<void()> callback);
  void region_clear(int32_t buffer_id);
  std::vector<Region> get_regions(int32_t buffer_id) const;

  // --- Cursor navigation (async, callback-based) ---
  void cursor_init(int32_t buffer_id, std::function<void(bool)> callback);
  void cursor_next(int32_t buffer_id, std::function<void()> callback);
  void cursor_prev(int32_t buffer_id, std::function<void()> callback);
  void cursor_move_dir(int32_t buffer_id, int dx, int dy, bool extend,
                       std::function<void()> callback);
  void cursor_activate(int32_t buffer_id);
  void cursor_clear(int32_t buffer_id);
  bool cursor_active(int32_t buffer_id) const;

  // --- Match list ---
  void match_set(int32_t buffer_id, const std::string& selector,
                 std::function<void(uint32_t count)> callback);
  void match_next(int32_t buffer_id, std::function<void()> callback);
  void match_prev(int32_t buffer_id, std::function<void()> callback);
  void match_clear(int32_t buffer_id);

  // --- Lua ---
  LuaRuntime *lua_runtime();

private:
  void ensure_cursor_elements(int32_t buffer_id, std::function<void()> continuation);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};
