#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_render_handler.h"

#include <iostream>

// Minimal render handler for offscreen rendering.
// Returns a 1x1 viewport for now — will be replaced with kitty graphics output.
class MinimalRenderHandler : public CefRenderHandler {
 public:
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, 800, 600);
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override {
    // Will be replaced with kitty graphics protocol output.
    // For now, just acknowledge we received a paint.
  }

 private:
  IMPLEMENT_REFCOUNTING(MinimalRenderHandler);
};

// Minimal client that wires up offscreen rendering and browser lifetime.
class MinimalClient : public CefClient,
                      public CefLifeSpanHandler {
 public:
  MinimalClient() : render_handler_(new MinimalRenderHandler()) {}

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefRenderHandler> GetRenderHandler() override {
    return render_handler_;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    std::cerr << "[cef] Browser created." << std::endl;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    std::cerr << "[cef] Browser closed, quitting message loop." << std::endl;
    CefQuitMessageLoop();
  }

 private:
  CefRefPtr<CefRenderHandler> render_handler_;
  IMPLEMENT_REFCOUNTING(MinimalClient);
};

// Minimal app — just handles process-level callbacks.
class MinimalApp : public CefApp, public CefBrowserProcessHandler {
 public:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  void OnContextInitialized() override {
    std::cerr << "[cef] Context initialized, creating offscreen browser."
              << std::endl;

    CefBrowserSettings browser_settings;
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0);

    CefBrowserHost::CreateBrowser(window_info, new MinimalClient(),
                                  "https://example.com", browser_settings,
                                  nullptr, nullptr);
  }

 private:
  IMPLEMENT_REFCOUNTING(MinimalApp);
};

int main(int argc, char* argv[]) {
  CefMainArgs main_args(argc, argv);
  CefRefPtr<MinimalApp> app(new MinimalApp());

  // Execute sub-processes (renderer, GPU, etc.) if this is a child process.
  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    return exit_code;
  }

  CefSettings settings;
  settings.windowless_rendering_enabled = true;
  settings.no_sandbox = true;

  if (!CefInitialize(main_args, settings, app, nullptr)) {
    std::cerr << "[cef] Failed to initialize." << std::endl;
    return CefGetExitCode();
  }

  std::cerr << "[cef] Running message loop..." << std::endl;
  CefRunMessageLoop();

  CefShutdown();
  std::cerr << "[cef] Shutdown complete." << std::endl;
  return 0;
}
