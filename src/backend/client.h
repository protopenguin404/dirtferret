
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_render_handler.h"
#include "minimal_renderer.h"

#include <iostream>

// Minimal client that wires up offscreen rendering and browser lifetime.
class MinimalClient : public CefClient, public CefLifeSpanHandler {
public:
  MinimalClient();

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
  CefRefPtr<CefRenderHandler> GetRenderHandler() override;

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

private:
  CefRefPtr<CefRenderHandler> render_handler_;
  IMPLEMENT_REFCOUNTING(MinimalClient);
};
