#pragma once

#include "include/cef_app.h"

#include <functional>

// CefApp + CefBrowserProcessHandler.
// Signals when CEF context is ready for buffer creation.
class App : public CefApp, public CefBrowserProcessHandler {
 public:
  using ReadyCallback = std::function<void()>;

  void set_ready_callback(ReadyCallback cb) { ready_callback_ = std::move(cb); }

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
  void OnContextInitialized() override;

 private:
  ReadyCallback ready_callback_;
  IMPLEMENT_REFCOUNTING(App);
};
