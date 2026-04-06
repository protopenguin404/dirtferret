#pragma once

#include "engine/renderer.h"
#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"

#include <string>

// CefClient that wires up offscreen rendering, browser lifetime,
// display events, and load events.
class Client : public CefClient,
               public CefLifeSpanHandler,
               public CefDisplayHandler,
               public CefLoadHandler {
 public:
  Client();

  // --- CefClient ---
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
  CefRefPtr<CefRenderHandler> GetRenderHandler() override;
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
  CefRefPtr<CefLoadHandler> GetLoadHandler() override;

  // --- CefLifeSpanHandler ---
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // --- CefDisplayHandler ---
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) override;
  void OnLoadingProgressChange(CefRefPtr<CefBrowser> browser,
                               double progress) override;

  // --- CefLoadHandler ---
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override;

  // --- Accessors ---
  CefRefPtr<CefBrowser> browser() const { return browser_; }
  CefRefPtr<Renderer> render_handler() const { return render_handler_; }
  std::string title() const { return title_; }
  std::string url() const { return url_; }
  double load_progress() const { return load_progress_; }
  bool is_loading() const { return is_loading_; }
  bool can_go_back() const { return can_go_back_; }
  bool can_go_forward() const { return can_go_forward_; }

 private:
  CefRefPtr<Renderer> render_handler_;
  CefRefPtr<CefBrowser> browser_;
  std::string title_;
  std::string url_;
  double load_progress_ = 0.0;
  bool is_loading_ = false;
  bool can_go_back_ = false;
  bool can_go_forward_ = false;
  IMPLEMENT_REFCOUNTING(Client);
};
