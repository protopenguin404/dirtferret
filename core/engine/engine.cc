#include "engine/engine.h"
#include "include/cef_app.h"

#include <iostream>

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::initialize(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    app_ = new App();

    // CEF child process check.
    int exit_code = CefExecuteProcess(main_args, app_.get(), nullptr);
    if (exit_code >= 0) {
        child_exit_code_ = exit_code;
        return false;
    }

    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;

    if (!CefInitialize(main_args, settings, app_.get(), nullptr)) {
        std::cerr << "[engine] Failed to initialize CEF." << std::endl;
        return false;
    }

    std::cerr << "[engine] CEF initialized." << std::endl;
    initialized_ = true;
    return true;
}

void Engine::do_message_loop_work() {
    if (initialized_) {
        CefDoMessageLoopWork();
    }
}

void Engine::shutdown() {
    if (initialized_) {
        CefShutdown();
        initialized_ = false;
        std::cerr << "[engine] CEF shutdown complete." << std::endl;
    }
}

void Engine::navigate(int32_t buffer_id, const std::string& url) {
    auto client = app_->client();
    if (!client || !client->browser()) return;

    // TODO: multi-buffer lookup by buffer_id
    std::cerr << "[engine] Navigating to: " << url << std::endl;
    client->browser()->GetMainFrame()->LoadURL(url);
}

void Engine::go_back(int32_t buffer_id) {
    auto client = app_->client();
    if (client && client->browser() && client->can_go_back())
        client->browser()->GoBack();
}

void Engine::go_forward(int32_t buffer_id) {
    auto client = app_->client();
    if (client && client->browser() && client->can_go_forward())
        client->browser()->GoForward();
}

void Engine::reload(int32_t buffer_id) {
    auto client = app_->client();
    if (client && client->browser())
        client->browser()->Reload();
}

void Engine::stop_load(int32_t buffer_id) {
    auto client = app_->client();
    if (client && client->browser())
        client->browser()->StopLoad();
}

std::string Engine::get_title(int32_t buffer_id) {
    auto client = app_->client();
    return client ? client->title() : "";
}

std::string Engine::get_url(int32_t buffer_id) {
    auto client = app_->client();
    return client ? client->url() : "";
}

BufferInfo Engine::get_buffer_info(int32_t buffer_id) {
    auto client = app_->client();
    if (!client) return {};

    BufferInfo info;
    info.id = client->browser() ? client->browser()->GetIdentifier() : 0;
    info.url = client->url();
    info.title = client->title();
    info.loading = client->is_loading();
    info.can_go_back = client->can_go_back();
    info.can_go_forward = client->can_go_forward();
    info.load_progress = client->load_progress();
    return info;
}

void Engine::resize(int32_t buffer_id, int width, int height) {
    auto client = app_->client();
    if (!client) return;

    client->render_handler()->set_view_size(width, height);
    if (client->browser())
        client->browser()->GetHost()->WasResized();
}

void Engine::set_frame_callback(FrameCallback cb) {
    frame_callback_ = std::move(cb);

    // Wire the callback through to the renderer.
    // OnContextInitialized is async, so we set it when the client exists.
    // TODO: handle the timing properly when client_ is null at this point.
    auto client = app_->client();
    if (client && client->render_handler()) {
        client->render_handler()->set_paint_callback(
            [this](const void* buf, int w, int h,
                   const CefRenderHandler::RectList&) {
                if (frame_callback_) {
                    // TODO: use actual buffer_id for multi-buffer
                    frame_callback_(0, buf, w, h);
                }
            });
    }
}
