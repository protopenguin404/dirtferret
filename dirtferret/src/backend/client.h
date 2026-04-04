
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_render_handler.h"
#include "minimal_renderer.h"

#include <iostream>
#include <string>

// Minimal client that wires up offscreen rendering, browser lifetime,
// and display events (title changes).
class MinimalClient : public CefClient,
                      public CefLifeSpanHandler,
                      public CefDisplayHandler {
public:
  MinimalClient();

  // --- CefClient ---
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
  CefRefPtr<CefRenderHandler> GetRenderHandler() override;
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;

  // --- CefLifeSpanHandler ---
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // --- CefDisplayHandler ---
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // --- Accessors ---
  CefRefPtr<CefBrowser> browser() const { return browser_; }
  std::string title() const { return title_; }

private:
  CefRefPtr<CefRenderHandler> render_handler_;
  CefRefPtr<CefBrowser> browser_;
  std::string title_;
  IMPLEMENT_REFCOUNTING(MinimalClient);
};
