#include "engine/engine.h"
#include "engine/app.h"
#include "engine/client.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "shm/frame_pool.h"

#include <cstring>
#include <iostream>
#include <map>

struct BufferState {
  CefRefPtr<Client> client;
  std::unique_ptr<FramePool> frame_pool;
  int32_t id = -1;
};

struct Engine::Impl {
  CefRefPtr<App> app;
  bool initialized = false;
  bool cef_ready = false;
  int child_exit_code = -1;

  std::map<int32_t, BufferState> buffers;
  int32_t active_buffer = -1;

  FrameCallback frame_callback;
  StateCallback state_callback;
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
  browser_settings.windowless_frame_rate = 30;

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
                           const CefRenderHandler::RectList &) {
          auto it = impl_->buffers.find(browser_id);
          if (it == impl_->buffers.end())
            return;
          auto &pool = it->second.frame_pool;
          if (!pool)
            return;

          if ((uint32_t)w != pool->width() || (uint32_t)h != pool->height()) {
            pool->resize(w, h);
          }
          std::memcpy(pool->write_buffer(), buf, pool->buffer_size());
          pool->swap();

          if (impl_->frame_callback) {
            impl_->frame_callback(browser_id, pool->read_buffer(), w, h);
          }
        });

    impl_->buffers[browser_id] = std::move(state);

    if (impl_->active_buffer < 0) {
      impl_->active_buffer = browser_id;
    }

    if (impl_->state_callback) {
      impl_->state_callback(browser_id);
    }
  });

  client->set_on_closed([this](int32_t browser_id) {
    std::cerr << "[engine] Buffer closed: " << browser_id << std::endl;
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

void Engine::set_frame_callback(FrameCallback cb) {
  impl_->frame_callback = std::move(cb);
}

void Engine::set_state_callback(StateCallback cb) {
  impl_->state_callback = std::move(cb);
}

LuaRuntime *Engine::lua_runtime() { return nullptr; } // Phase 5

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
