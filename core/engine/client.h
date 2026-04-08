#pragma once

#include "engine/renderer.h"
#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"

#include <functional>
#include <string>

class Client : public CefClient,
               public CefLifeSpanHandler,
               public CefDisplayHandler,
               public CefLoadHandler {
 public:
  using OnCreatedCallback = std::function<void(CefRefPtr<Client>, int32_t)>;
  using OnClosedCallback = std::function<void(int32_t)>;
  using OnStateChangedCallback = std::function<void(int32_t)>;

  Client();

  void set_on_created(OnCreatedCallback cb) { on_created_ = std::move(cb); }
  void set_on_closed(OnClosedCallback cb) { on_closed_ = std::move(cb); }
  void set_on_state_changed(OnStateChangedCallback cb) {
    on_state_changed_ = std::move(cb);
  }

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
  int32_t buffer_id() const { return buffer_id_; }
  std::string title() const { return title_; }
  std::string url() const { return url_; }
  double load_progress() const { return load_progress_; }
  bool is_loading() const { return is_loading_; }
  bool can_go_back() const { return can_go_back_; }
  bool can_go_forward() const { return can_go_forward_; }

 private:
  CefRefPtr<Renderer> render_handler_;
  CefRefPtr<CefBrowser> browser_;
  int32_t buffer_id_ = -1;
  std::string title_;
  std::string url_;
  double load_progress_ = 0.0;
  bool is_loading_ = false;
  bool can_go_back_ = false;
  bool can_go_forward_ = false;

  OnCreatedCallback on_created_;
  OnClosedCallback on_closed_;
  OnStateChangedCallback on_state_changed_;

  IMPLEMENT_REFCOUNTING(Client);
};
