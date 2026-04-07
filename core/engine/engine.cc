#include "engine/engine.h"
#include "engine/app.h"
#include "include/cef_app.h"
#include "shm/frame_pool.h"

#include <iostream>

struct Engine::Impl {
  CefRefPtr<App> app;
  bool initialized = false;
  int child_exit_code = -1;
  FrameCallback frame_callback;
  std::unique_ptr<FramePool> frame_pool;
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() = default;

int Engine::child_exit_code() const { return impl_->child_exit_code; }

bool Engine::initialize(int argc, char *argv[]) {
  CefMainArgs main_args(argc, argv);
  impl_->app = new App();

  // CEF child process check.
  int exit_code = CefExecuteProcess(main_args, impl_->app.get(), nullptr);
  if (exit_code >= 0) {
    impl_->child_exit_code = exit_code;
    return false;
  }

  CefSettings settings;
  settings.windowless_rendering_enabled = true;
  settings.no_sandbox = true;

  if (!CefInitialize(main_args, settings, impl_->app.get(), nullptr)) {
    std::cerr << "[engine] Failed to initialize CEF." << std::endl;
    return false;
  }

  std::cerr << "[engine] CEF initialized." << std::endl;
  impl_->initialized = true;
  return true;
}

bool Engine::setup_frame_pool(uint32_t width, uint32_t height) {

  // Setup the frame pool
  impl_->frame_pool = std::make_unique<FramePool>("dirtferret", width, height);
  if (!impl_->frame_pool->create()) {
    std::cerr << "[engine] Failed to create frame pool" << std ::endl;
    return false;
  }
  std::cerr << "[engine] Frame pool created: " << width << "x" << height
            << std::endl;

  // Configure the frame callback
  auto client = impl_->app->client();
  if (client && client->render_handler()) {
    client->render_handler()->set_paint_callback(
        [this](const void *buf, int w, int h,
               const CefRenderHandler::RectList &) {
          auto &pool = impl_->frame_pool;
          if (!pool)
            return;
          if ((uint32_t)w != pool->width() || (uint32_t)h != pool->height()) {
            pool->resize(w, h);
          }

          memcpy(pool->write_buffer(), buf, pool->buffer_size());
          pool->swap();

          if (impl_->frame_callback) {
            impl_->frame_callback(0, pool->read_buffer(), w, h);
          }
        });
  }
  return true;
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

int32_t Engine::create_buffer(int stub) { return 0; } // Stub

void Engine::navigate(int32_t buffer_id, const std::string &url) {
  auto client = impl_->app->client();
  if (!client || !client->browser())
    return;

  // TODO: multi-buffer lookup by buffer_id
  std::cerr << "[engine] Navigating to: " << url << std::endl;
  client->browser()->GetMainFrame()->LoadURL(url);
}

void Engine::go_back(int32_t buffer_id) {
  auto client = impl_->app->client();
  if (client && client->browser() && client->can_go_back())
    client->browser()->GoBack();
}

void Engine::go_forward(int32_t buffer_id) {
  auto client = impl_->app->client();
  if (client && client->browser() && client->can_go_forward())
    client->browser()->GoForward();
}

void Engine::reload(int32_t buffer_id) {
  auto client = impl_->app->client();
  if (client && client->browser())
    client->browser()->Reload();
}

void Engine::stop_load(int32_t buffer_id) {
  auto client = impl_->app->client();
  if (client && client->browser())
    client->browser()->StopLoad();
}

std::string Engine::get_title(int32_t buffer_id) {
  auto client = impl_->app->client();
  return client ? client->title() : "";
}

std::string Engine::get_url(int32_t buffer_id) {
  auto client = impl_->app->client();
  return client ? client->url() : "";
}

std::string Engine::frame_shm_name() const {
  if (impl_->frame_pool)
    return impl_->frame_pool->read_shm_name();
  return "";
}

void Engine::resize(int32_t buffer_id, int width, int height) {
  auto client = impl_->app->client();
  if (!client)
    return;

  client->render_handler()->set_view_size(width, height);
  if (client->browser())
    client->browser()->GetHost()->WasResized();
}

void Engine::set_frame_callback(FrameCallback cb) {
  impl_->frame_callback = std::move(cb);

  // Wire the callback through to the renderer.
  // OnContextInitialized is async, so we set it when the client exists.
  // TODO: handle the timing properly when client_ is null at this point.
  auto client = impl_->app->client();
  if (client && client->render_handler()) {
    client->render_handler()->set_paint_callback(
        [this](const void *buf, int w, int h,
               const CefRenderHandler::RectList &) {
          if (impl_->frame_callback) {
            // TODO: use actual buffer_id for multi-buffer
            impl_->frame_callback(0, buf, w, h);
          }
        });
  }
}
