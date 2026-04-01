#include "backend/engine.h"
#include "api/dispatcher.h"
#include "include/cef_app.h"

#include <iostream>

namespace cef_terminal {

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::initialize(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    app_ = new MinimalApp();

    // CEF child process check — if this is a renderer/GPU/utility process,
    // CefExecuteProcess handles it and returns >= 0. We must exit.
    int exit_code = CefExecuteProcess(main_args, app_.get(), nullptr);
    if (exit_code >= 0) {
        // We are a CEF child process. Caller should exit with this code.
        child_exit_code_ = exit_code;
        return false;
    }

    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;

    if (!CefInitialize(main_args, settings, app_.get(), nullptr)) {
        std::cerr << "[cef] Failed to initialize." << std::endl;
        return false;
    }

    std::cerr << "[cef] Initialized." << std::endl;
    initialized_ = true;
    return true;
}

void Engine::tick() {
    if (initialized_) {
        CefDoMessageLoopWork();
    }
}

void Engine::shutdown() {
    if (initialized_) {
        CefShutdown();
        initialized_ = false;
        std::cerr << "[cef] Shutdown complete." << std::endl;
    }
}

void Engine::register_handlers(Dispatcher& dispatcher) {
    dispatcher.register_command("buffer.navigate",
        [this](const Command& cmd) -> CommandResult {
            auto client = app_->client();
            if (!client || !client->browser()) {
                return CommandResult::failure("no browser available");
            }

            auto it = cmd.args.find("url");
            if (it == cmd.args.end()) {
                return CommandResult::failure("missing 'url' arg");
            }

            auto* url = std::get_if<std::string>(&it->second);
            if (!url) {
                return CommandResult::failure("'url' must be a string");
            }

            std::cerr << "[cef] Navigating to: " << *url << std::endl;
            client->browser()->GetMainFrame()->LoadURL(*url);
            return CommandResult::success();
        });

    dispatcher.register_query("buffer.get_title",
        [this](const Query&) -> QueryResult {
            auto client = app_->client();
            if (!client) {
                return QueryResult::failure("no client available");
            }

            Args data;
            data["title"] = client->title();
            return QueryResult::success(std::move(data));
        });
}

} // namespace cef_terminal
