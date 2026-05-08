#include "engine/engine.h"
#include "engine/app.h"
#include "engine/client.h"
#include "engine/dom_bridge.h"
#include "engine/input.h"
#include "lua/runtime.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "shm/frame_pool.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <map>

static const char* FOCUSABLE_SELECTOR =
    "a[href],button,input,select,textarea,[tabindex],[role='button'],[onclick]";

struct CursorState {
  bool active = false;
  uint32_t region_id = 0;
  int32_t current_index = -1;
  std::vector<ElementInfo> cached_elements;
  bool elements_dirty = true;
};

struct MatchList {
  std::vector<ElementInfo> matches;
  int32_t current_index = -1;
  bool active = false;
};

struct BufferState {
  CefRefPtr<Client> client;
  std::unique_ptr<FramePool> frame_pool;
  int32_t id = -1;
  RegionSet regions;
  CefRefPtr<DomBridge> dom_bridge;
  CursorState cursor;
  MatchList match_list;
  int32_t last_mouse_x = 0;
  int32_t last_mouse_y = 0;
};

struct Engine::Impl {
  CefRefPtr<App> app;
  bool initialized = false;
  bool cef_ready = false;
  int child_exit_code = -1;

  std::map<int32_t, BufferState> buffers;
  int32_t active_buffer = -1;

  StateCallback state_callback;
  BufferCreatedCallback buffer_created_callback;
  BufferClosedCallback buffer_closed_callback;

  std::unique_ptr<LuaRuntime> lua;
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() = default;

int Engine::child_exit_code() const { return impl_->child_exit_code; }

bool Engine::initialize(int argc, char *argv[]) {
  CefMainArgs main_args(argc, argv);
  impl_->app = new App();

  int exit_code = CefExecuteProcess(main_args, impl_->app.get(), nullptr);
  if (exit_code >= 0) {
    impl_->child_exit_code = exit_code;
    return false;
  }

  CefSettings settings;
  settings.windowless_rendering_enabled = true;
  settings.no_sandbox = true;
  CefString(&settings.root_cache_path) = "/tmp/dirtferret";

  impl_->app->set_ready_callback([this]() {
    impl_->cef_ready = true;
    std::cerr << "[engine] CEF ready for buffer creation." << std::endl;
  });

  if (!CefInitialize(main_args, settings, impl_->app.get(), nullptr)) {
    std::cerr << "[engine] Failed to initialize CEF." << std::endl;
    return false;
  }

  std::cerr << "[engine] CEF initialized." << std::endl;
  impl_->initialized = true;

  impl_->lua = std::make_unique<LuaRuntime>();
  if (!impl_->lua->initialize()) {
    std::cerr << "[engine] Lua init failed (non-fatal)." << std::endl;
  }

  return true;
}

int32_t Engine::create_buffer(const std::string &url,
                              uint32_t viewport_width,
                              uint32_t viewport_height) {
  if (!impl_->cef_ready) {
    std::cerr << "[engine] CEF not ready, cannot create buffer." << std::endl;
    return -1;
  }

  auto client = CefRefPtr<Client>(new Client());

  CefBrowserSettings browser_settings;
  browser_settings.windowless_frame_rate = 60;

  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);

  client->render_handler()->set_view_size(viewport_width, viewport_height);

  // Client notifies us via callback when browser is created.
  client->set_on_created([this, viewport_width, viewport_height](
                             CefRefPtr<Client> c, int32_t browser_id) {
    std::cerr << "[engine] Buffer created: " << browser_id << std::endl;

    BufferState state;
    state.client = c;
    state.id = browser_id;

    // Create per-buffer frame pool
    std::string prefix = "dirtferret-" + std::to_string(browser_id);
    state.frame_pool =
        std::make_unique<FramePool>(prefix, viewport_width, viewport_height);
    if (!state.frame_pool->create()) {
      std::cerr << "[engine] Failed to create frame pool for buffer "
                << browser_id << std::endl;
      return;
    }

    // Wire OnPaint to frame pool
    c->render_handler()->set_paint_callback(
        [this, browser_id](const void *buf, int w, int h,
                           const CefRenderHandler::RectList &dirtyRects) {
          auto it = impl_->buffers.find(browser_id);
          if (it == impl_->buffers.end())
            return;
          auto &pool = it->second.frame_pool;
          if (!pool)
            return;

          if ((uint32_t)w != pool->width() || (uint32_t)h != pool->height()) {
            if (!pool->resize(w, h)) {
              std::cerr << "[engine] Frame pool resize failed for buffer "
                        << browser_id << std::endl;
              return;
            }
          }
          if (!pool->write_buffer())
            return;
          std::memcpy(pool->write_buffer(), buf, pool->buffer_size());

          // Convert CefRects to NotifyDirtyRects and publish
          std::vector<NotifyDirtyRect> rects;
          rects.reserve(dirtyRects.size());
          for (auto &r : dirtyRects) {
            rects.push_back({r.x, r.y, (uint32_t)r.width, (uint32_t)r.height});
          }
          pool->publish(rects);
        });

    // Initialize DOM bridge for CDP access
    state.dom_bridge = new DomBridge(c->browser());
    state.dom_bridge->enable();

    // Wire document-updated to dirty cursor elements cache
    state.dom_bridge->set_on_document_updated([this, browser_id]() {
      auto it2 = impl_->buffers.find(browser_id);
      if (it2 != impl_->buffers.end()) {
        it2->second.cursor.elements_dirty = true;
      }
    });

    impl_->buffers[browser_id] = std::move(state);

    if (impl_->active_buffer < 0) {
      impl_->active_buffer = browser_id;
    }

    // Notify buffer creation BEFORE state updates so TUI registers the buffer first
    if (impl_->buffer_created_callback) {
      impl_->buffer_created_callback(browser_id);
    }

    if (impl_->state_callback) {
      impl_->state_callback(browser_id);
    }
  });

  client->set_on_closed([this](int32_t browser_id) {
    std::cerr << "[engine] Buffer closed: " << browser_id << std::endl;

    // Disable DomBridge observer before destroying BufferState
    // to prevent dangling callbacks from in-flight CDP responses.
    auto it = impl_->buffers.find(browser_id);
    if (it != impl_->buffers.end() && it->second.dom_bridge) {
      it->second.dom_bridge->disable();
    }

    if (impl_->buffer_closed_callback) {
      impl_->buffer_closed_callback(browser_id);
    }

    impl_->buffers.erase(browser_id);

    if (impl_->active_buffer == browser_id) {
      if (!impl_->buffers.empty()) {
        impl_->active_buffer = impl_->buffers.begin()->first;
      } else {
        impl_->active_buffer = -1;
      }
    }
  });

  client->set_on_state_changed([this](int32_t browser_id) {
    auto it = impl_->buffers.find(browser_id);
    if (it != impl_->buffers.end()) {
      // If loading just finished, dirty the cursor elements cache
      if (!it->second.client->is_loading()) {
        it->second.cursor.elements_dirty = true;
      }
    }
    if (impl_->state_callback) {
      impl_->state_callback(browser_id);
    }
  });

  CefBrowserHost::CreateBrowser(window_info, client, url,
                                browser_settings, nullptr, nullptr);
  return 0; // Real ID comes async via OnAfterCreated
}

void Engine::close_buffer(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  auto browser = it->second.client->browser();
  if (browser) {
    browser->GetHost()->CloseBrowser(true);
  }
}

size_t Engine::buffer_count() const { return impl_->buffers.size(); }

void Engine::navigate(int32_t buffer_id, const std::string &url) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  auto browser = it->second.client->browser();
  if (browser) {
    std::cerr << "[engine] Navigating buffer " << buffer_id << " to: " << url
              << std::endl;
    browser->GetMainFrame()->LoadURL(url);
  }
}

void Engine::go_back(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  auto &c = it->second.client;
  if (c->browser() && c->can_go_back())
    c->browser()->GoBack();
}

void Engine::go_forward(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  auto &c = it->second.client;
  if (c->browser() && c->can_go_forward())
    c->browser()->GoForward();
}

void Engine::reload(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  if (it->second.client->browser())
    it->second.client->browser()->Reload();
}

void Engine::stop_load(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  if (it->second.client->browser())
    it->second.client->browser()->StopLoad();
}

std::string Engine::get_title(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return "";
  return it->second.client->title();
}

std::string Engine::get_url(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return "";
  return it->second.client->url();
}

bool Engine::is_loading(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return false;
  return it->second.client->is_loading();
}

bool Engine::can_go_back(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return false;
  return it->second.client->can_go_back();
}

bool Engine::can_go_forward(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return false;
  return it->second.client->can_go_forward();
}

double Engine::load_progress(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return 0.0;
  return it->second.client->load_progress();
}

std::string Engine::frame_shm_name(int32_t buffer_id) const {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return "";
  if (it->second.frame_pool)
    return it->second.frame_pool->read_shm_name();
  return "";
}

int32_t Engine::active_buffer_id() const { return impl_->active_buffer; }

void Engine::set_active_buffer(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;

  if (impl_->active_buffer >= 0 && impl_->active_buffer != buffer_id) {
    auto prev = impl_->buffers.find(impl_->active_buffer);
    if (prev != impl_->buffers.end() && prev->second.client->browser()) {
      prev->second.client->browser()->GetHost()->WasHidden(true);
    }
  }

  impl_->active_buffer = buffer_id;

  if (it->second.client->browser()) {
    it->second.client->browser()->GetHost()->WasHidden(false);
  }
}

std::vector<int32_t> Engine::list_buffer_ids() const {
  std::vector<int32_t> ids;
  ids.reserve(impl_->buffers.size());
  for (auto &[id, _] : impl_->buffers) {
    ids.push_back(id);
  }
  return ids;
}

void Engine::resize(int32_t buffer_id, int width, int height) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  it->second.client->render_handler()->set_view_size(width, height);
  if (it->second.client->browser())
    it->second.client->browser()->GetHost()->WasResized();
}

void Engine::set_state_callback(StateCallback cb) {
  impl_->state_callback = std::move(cb);
}

void Engine::set_buffer_created_callback(BufferCreatedCallback cb) {
  impl_->buffer_created_callback = std::move(cb);
}

void Engine::set_buffer_closed_callback(BufferClosedCallback cb) {
  impl_->buffer_closed_callback = std::move(cb);
}

LuaRuntime *Engine::lua_runtime() { return impl_->lua.get(); }

void Engine::send_key_event(int32_t buffer_id, uint32_t key_type,
                            uint32_t key_code, uint32_t character,
                            uint32_t modifiers) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  auto browser = it->second.client->browser();
  if (!browser)
    return;

  auto translated = translate_key(character, modifiers);

  CefKeyEvent event;
  event.windows_key_code = translated.windows_key_code;
  event.native_key_code = translated.native_key_code;
  event.character = translated.character;
  event.unmodified_character = translated.unmodified_character;
  event.modifiers = translated.modifiers;

  switch (key_type) {
  case 0: event.type = KEYEVENT_RAWKEYDOWN; break;
  case 1: event.type = KEYEVENT_KEYDOWN; break;
  case 2: event.type = KEYEVENT_KEYUP; break;
  case 3: event.type = KEYEVENT_CHAR; break;
  default: event.type = KEYEVENT_RAWKEYDOWN;
  }

  browser->GetHost()->SendKeyEvent(event);
}

void Engine::send_mouse_event(int32_t buffer_id, uint32_t event_type,
                              int x, int y, uint32_t button,
                              uint32_t modifiers) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;

  it->second.last_mouse_x = x;
  it->second.last_mouse_y = y;

  auto browser = it->second.client->browser();
  if (!browser)
    return;

  CefMouseEvent event;
  event.x = x;
  event.y = y;
  event.modifiers = 0;
  if (modifiers & MODIFIER_SHIFT) event.modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (modifiers & MODIFIER_CTRL) event.modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (modifiers & MODIFIER_ALT) event.modifiers |= EVENTFLAG_ALT_DOWN;

  cef_mouse_button_type_t cef_button;
  switch (button) {
  case 0: cef_button = MBT_LEFT; break;
  case 1: cef_button = MBT_MIDDLE; break;
  case 2: cef_button = MBT_RIGHT; break;
  default: cef_button = MBT_LEFT;
  }

  if (event_type == 2) {
    browser->GetHost()->SendMouseMoveEvent(event, false);
  } else {
    bool mouse_up = (event_type == 1);
    browser->GetHost()->SendMouseClickEvent(event, cef_button, mouse_up, 1);
  }
}

void Engine::send_scroll_event(int32_t buffer_id, int delta_x, int delta_y) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end())
    return;
  auto browser = it->second.client->browser();
  if (!browser)
    return;

  auto translated = translate_scroll(0, 0, delta_x, delta_y);
  CefMouseEvent event;
  event.x = 0;
  event.y = 0;
  event.modifiers = 0;
  browser->GetHost()->SendMouseWheelEvent(event, translated.delta_x, translated.delta_y);
}

// --- DOM queries ---

void Engine::element_at(int32_t buffer_id, int x, int y,
                        std::function<void(ElementInfo)> callback) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end() || !it->second.dom_bridge) {
    callback(ElementInfo{});
    return;
  }
  it->second.dom_bridge->element_at(x, y, std::move(callback));
}

void Engine::query(int32_t buffer_id, const std::string& selector,
                   std::function<void(std::vector<ElementInfo>)> callback) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end() || !it->second.dom_bridge) {
    callback({});
    return;
  }
  it->second.dom_bridge->query(selector, std::move(callback));
}

// --- Region management ---

uint32_t Engine::region_add(int32_t buffer_id, int x, int y) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return 0;
  auto rid = it->second.regions.add({x, y});
  if (it->second.dom_bridge) {
    it->second.dom_bridge->inject_cursor_visual(rid, x, y);
  }
  return rid;
}

void Engine::region_remove(int32_t buffer_id, uint32_t region_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return;
  it->second.regions.remove(region_id);
  if (it->second.dom_bridge) {
    it->second.dom_bridge->remove_cursor_visual(region_id);
  }
}

void Engine::region_move(int32_t buffer_id, uint32_t region_id, int x, int y) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return;
  it->second.regions.move(region_id, {x, y});
  if (it->second.dom_bridge) {
    it->second.dom_bridge->inject_cursor_visual(region_id, x, y);
  }
}

void Engine::region_select(int32_t buffer_id, Scope scope,
                           const std::string& selector_arg,
                           std::function<void()> callback) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) {
    if (callback) callback();
    return;
  }
  auto& buf = it->second;
  if (!buf.dom_bridge || buf.regions.count() == 0) {
    if (callback) callback();
    return;
  }

  // For v1: only Element scope. Others return immediately.
  if (scope != Scope::Element) {
    std::cerr << "[engine] region_select: scope not yet implemented\n";
    if (callback) callback();
    return;
  }

  // Process first region only for v1
  auto* primary = buf.regions.primary();
  if (!primary) {
    if (callback) callback();
    return;
  }

  uint32_t rid = primary->id;
  int hx = primary->head.x;
  int hy = primary->head.y;

  buf.dom_bridge->element_at(hx, hy,
      [this, buffer_id, rid, cb = std::move(callback)](ElementInfo info) {
        auto it2 = impl_->buffers.find(buffer_id);
        if (it2 == impl_->buffers.end()) {
          if (cb) cb();
          return;
        }

        if (info.node_id >= 0 && info.bounds_width > 0) {
          Point anchor = {info.bounds_x, info.bounds_y};
          Point head = {
              static_cast<int32_t>(info.bounds_x + info.bounds_width),
              static_cast<int32_t>(info.bounds_y + info.bounds_height)
          };
          it2->second.regions.set_selection(rid, anchor, head);

          if (it2->second.dom_bridge) {
            it2->second.dom_bridge->highlight_node(
                info.node_id, 0, 120, 255, 0.3f);
          }
        }

        if (cb) cb();
      });
}

void Engine::region_clear(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return;
  it->second.regions.clear();
  if (it->second.dom_bridge) {
    it->second.dom_bridge->clear_all_visuals();
  }
}

std::vector<Region> Engine::get_regions(int32_t buffer_id) const {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return {};
  return it->second.regions.all();
}

// --- Spatial navigation helper (testable free function) ---

int find_nearest_in_direction(const std::vector<ElementInfo>& elements,
                              int current_index, int dx, int dy) {
  if (current_index < 0 || current_index >= (int)elements.size()) return -1;

  auto& current = elements[current_index];
  int cx = current.bounds_x + (int)current.bounds_width / 2;
  int cy = current.bounds_y + (int)current.bounds_height / 2;

  int best_index = -1;
  double best_dist = 1e18;

  for (size_t i = 0; i < elements.size(); ++i) {
    if ((int)i == current_index) continue;
    auto& cand = elements[i];
    int ex = cand.bounds_x + (int)cand.bounds_width / 2;
    int ey = cand.bounds_y + (int)cand.bounds_height / 2;

    int delta_x = ex - cx;
    int delta_y = ey - cy;

    // Check forward half-plane: dot product with direction > 0
    int dot = delta_x * dx + delta_y * dy;
    if (dot <= 0) continue;

    double dist = std::sqrt((double)(delta_x * delta_x + delta_y * delta_y));
    if (dist < best_dist) {
      best_dist = dist;
      best_index = (int)i;
    }
  }

  return best_index;
}

// --- Cursor navigation ---

void Engine::ensure_cursor_elements(int32_t buffer_id,
                                    std::function<void()> continuation) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end() || !it->second.dom_bridge) {
    continuation();
    return;
  }
  auto& cursor = it->second.cursor;
  if (!cursor.elements_dirty) {
    continuation();
    return;
  }
  it->second.dom_bridge->query(FOCUSABLE_SELECTOR,
      [this, buffer_id, cont = std::move(continuation)](std::vector<ElementInfo> elements) {
        auto it2 = impl_->buffers.find(buffer_id);
        if (it2 != impl_->buffers.end()) {
          it2->second.cursor.cached_elements = std::move(elements);
          it2->second.cursor.elements_dirty = false;
        }
        cont();
      });
}

void Engine::cursor_init(int32_t buffer_id, std::function<void(bool)> callback) {
  ensure_cursor_elements(buffer_id, [this, buffer_id, cb = std::move(callback)]() {
    auto it = impl_->buffers.find(buffer_id);
    if (it == impl_->buffers.end()) { cb(false); return; }
    auto& buf = it->second;
    auto& cursor = buf.cursor;

    if (cursor.cached_elements.empty()) { cb(false); return; }

    cursor.current_index = 0;
    cursor.active = true;

    auto& elem = cursor.cached_elements[0];
    int cx = elem.bounds_x + (int)elem.bounds_width / 2;
    int cy = elem.bounds_y + (int)elem.bounds_height / 2;

    // Clear any old cursor region
    if (cursor.region_id > 0) {
      buf.regions.remove(cursor.region_id);
    }
    cursor.region_id = buf.regions.add({cx, cy});

    // Highlight
    if (buf.dom_bridge) {
      buf.dom_bridge->clear_highlight();
      buf.dom_bridge->highlight_node(elem.node_id, 66, 135, 245, 0.4f);
    }

    cb(true);
  });
}

void Engine::cursor_next(int32_t buffer_id, std::function<void()> callback) {
  ensure_cursor_elements(buffer_id, [this, buffer_id, cb = std::move(callback)]() {
    auto it = impl_->buffers.find(buffer_id);
    if (it == impl_->buffers.end()) { if (cb) cb(); return; }
    auto& buf = it->second;
    auto& cursor = buf.cursor;

    if (!cursor.active || cursor.cached_elements.empty()) { if (cb) cb(); return; }

    cursor.current_index = (cursor.current_index + 1) % (int)cursor.cached_elements.size();

    auto& elem = cursor.cached_elements[cursor.current_index];
    int cx = elem.bounds_x + (int)elem.bounds_width / 2;
    int cy = elem.bounds_y + (int)elem.bounds_height / 2;

    buf.regions.move(cursor.region_id, {cx, cy});

    if (buf.dom_bridge) {
      buf.dom_bridge->clear_highlight();
      buf.dom_bridge->highlight_node(elem.node_id, 66, 135, 245, 0.4f);
    }

    if (cb) cb();
  });
}

void Engine::cursor_prev(int32_t buffer_id, std::function<void()> callback) {
  ensure_cursor_elements(buffer_id, [this, buffer_id, cb = std::move(callback)]() {
    auto it = impl_->buffers.find(buffer_id);
    if (it == impl_->buffers.end()) { if (cb) cb(); return; }
    auto& buf = it->second;
    auto& cursor = buf.cursor;

    if (!cursor.active || cursor.cached_elements.empty()) { if (cb) cb(); return; }

    int size = (int)cursor.cached_elements.size();
    cursor.current_index = (cursor.current_index - 1 + size) % size;

    auto& elem = cursor.cached_elements[cursor.current_index];
    int cx = elem.bounds_x + (int)elem.bounds_width / 2;
    int cy = elem.bounds_y + (int)elem.bounds_height / 2;

    buf.regions.move(cursor.region_id, {cx, cy});

    if (buf.dom_bridge) {
      buf.dom_bridge->clear_highlight();
      buf.dom_bridge->highlight_node(elem.node_id, 66, 135, 245, 0.4f);
    }

    if (cb) cb();
  });
}

void Engine::cursor_move_dir(int32_t buffer_id, int dx, int dy, bool extend,
                             std::function<void()> callback) {
  ensure_cursor_elements(buffer_id,
      [this, buffer_id, dx, dy, extend, cb = std::move(callback)]() {
    auto it = impl_->buffers.find(buffer_id);
    if (it == impl_->buffers.end()) { if (cb) cb(); return; }
    auto& buf = it->second;
    auto& cursor = buf.cursor;

    if (!cursor.active || cursor.cached_elements.empty() || cursor.current_index < 0) {
      if (cb) cb();
      return;
    }

    int best_index = find_nearest_in_direction(
        cursor.cached_elements, cursor.current_index, dx, dy);

    if (best_index >= 0) {
      cursor.current_index = best_index;
      auto& elem = cursor.cached_elements[best_index];
      int nx = elem.bounds_x + (int)elem.bounds_width / 2;
      int ny = elem.bounds_y + (int)elem.bounds_height / 2;

      if (extend) {
        buf.regions.extend(cursor.region_id, {nx, ny});
      } else {
        buf.regions.move(cursor.region_id, {nx, ny});
      }

      if (buf.dom_bridge) {
        buf.dom_bridge->clear_highlight();
        buf.dom_bridge->highlight_node(elem.node_id, 66, 135, 245, 0.4f);
      }
    }

    if (cb) cb();
  });
}

void Engine::cursor_activate(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return;
  auto& cursor = it->second.cursor;
  if (!cursor.active || cursor.current_index < 0 ||
      cursor.current_index >= (int)cursor.cached_elements.size()) return;

  auto& elem = cursor.cached_elements[cursor.current_index];
  int cx = elem.bounds_x + (int)elem.bounds_width / 2;
  int cy = elem.bounds_y + (int)elem.bounds_height / 2;

  // Click: mouse down then up
  send_mouse_event(buffer_id, 0/*down*/, cx, cy, 0/*left*/, 0/*no mods*/);
  send_mouse_event(buffer_id, 1/*up*/, cx, cy, 0/*left*/, 0/*no mods*/);
}

void Engine::cursor_clear(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return;
  auto& cursor = it->second.cursor;
  if (cursor.region_id > 0) {
    it->second.regions.remove(cursor.region_id);
  }
  if (it->second.dom_bridge) {
    it->second.dom_bridge->clear_highlight();
  }
  cursor.active = false;
  cursor.region_id = 0;
  cursor.current_index = -1;
  // Also clear match list
  match_clear(buffer_id);
}

bool Engine::cursor_active(int32_t buffer_id) const {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return false;
  return it->second.cursor.active;
}

// --- Match list ---

void Engine::match_set(int32_t buffer_id, const std::string& selector,
                       std::function<void(uint32_t count)> callback) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end() || !it->second.dom_bridge) {
    callback(0);
    return;
  }

  it->second.dom_bridge->query(selector,
      [this, buffer_id, cb = std::move(callback)](std::vector<ElementInfo> elements) {
        auto it2 = impl_->buffers.find(buffer_id);
        if (it2 == impl_->buffers.end()) { cb(0); return; }
        auto& buf = it2->second;

        buf.match_list.matches = std::move(elements);
        buf.match_list.active = !buf.match_list.matches.empty();
        buf.match_list.current_index = buf.match_list.active ? 0 : -1;

        // Also set cursor to first match
        if (buf.match_list.active) {
          buf.cursor.cached_elements = buf.match_list.matches;
          buf.cursor.elements_dirty = false;
          buf.cursor.current_index = 0;
          buf.cursor.active = true;

          auto& elem = buf.match_list.matches[0];
          int cx = elem.bounds_x + (int)elem.bounds_width / 2;
          int cy = elem.bounds_y + (int)elem.bounds_height / 2;

          if (buf.cursor.region_id > 0) {
            buf.regions.remove(buf.cursor.region_id);
          }
          buf.cursor.region_id = buf.regions.add({cx, cy});

          if (buf.dom_bridge) {
            buf.dom_bridge->clear_highlight();
            buf.dom_bridge->highlight_node(elem.node_id, 66, 135, 245, 0.4f);
          }
        }

        cb(static_cast<uint32_t>(buf.match_list.matches.size()));
      });
}

void Engine::match_next(int32_t buffer_id, std::function<void()> callback) {
  cursor_next(buffer_id, [this, buffer_id, cb = std::move(callback)]() {
    auto it = impl_->buffers.find(buffer_id);
    if (it != impl_->buffers.end() && it->second.match_list.active) {
      it->second.match_list.current_index = it->second.cursor.current_index;
    }
    if (cb) cb();
  });
}

void Engine::match_prev(int32_t buffer_id, std::function<void()> callback) {
  cursor_prev(buffer_id, [this, buffer_id, cb = std::move(callback)]() {
    auto it = impl_->buffers.find(buffer_id);
    if (it != impl_->buffers.end() && it->second.match_list.active) {
      it->second.match_list.current_index = it->second.cursor.current_index;
    }
    if (cb) cb();
  });
}

void Engine::match_clear(int32_t buffer_id) {
  auto it = impl_->buffers.find(buffer_id);
  if (it == impl_->buffers.end()) return;
  it->second.match_list.matches.clear();
  it->second.match_list.current_index = -1;
  it->second.match_list.active = false;
}

void Engine::do_message_loop_work() {
  if (impl_->initialized) {
    CefDoMessageLoopWork();
  }
}

void Engine::shutdown() {
  if (impl_->initialized) {
    CefShutdown();
    impl_->initialized = false;
    std::cerr << "[engine] CEF shutdown complete." << std::endl;
  }
}
