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

ExportManifest Engine::exports() {
    ExportManifest manifest;
    manifest.ns = "buffer";
    manifest.description = "Browser buffer management — navigation, titles, lifecycle.";

    // --- Commands ---

    manifest.commands.push_back({
        "navigate",
        "Navigate the active buffer to a URL.",
        {{"url", "string", true, "The URL to load."}},
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
        }
    });

    // --- Queries ---

    manifest.queries.push_back({
        "get_title",
        "Get the current page title of the active buffer.",
        {},  // no args
        {{"title", "string", true, "The page title."}},
        [this](const Query&) -> QueryResult {
            auto client = app_->client();
            if (!client) {
                return QueryResult::failure("no client available");
            }

            Args data;
            data["title"] = client->title();
            return QueryResult::success(std::move(data));
        }
    });

    // --- Events (declared, not yet wired) ---

    manifest.events.push_back({
        "load_finished",
        "Fired when a page finishes loading.",
        {{"url", "string", true, "The URL that finished loading."},
         {"status_code", "int", true, "HTTP status code."}}
    });

    manifest.events.push_back({
        "title_changed",
        "Fired when the page title changes.",
        {{"title", "string", true, "The new title."}}
    });

    // --- Properties ---

    manifest.properties.push_back({
        "title",
        "The current page title.",
        "string",
        false,  // read-only
        [this](const Query&) -> QueryResult {
            auto client = app_->client();
            if (!client) {
                return QueryResult::failure("no client available");
            }
            Args data;
            data["value"] = client->title();
            return QueryResult::success(std::move(data));
        },
        nullptr  // no setter
    });

    return manifest;
}

void Engine::register_handlers(Dispatcher& dispatcher) {
    dispatcher.register_exports(exports());
}

} // namespace cef_terminal
