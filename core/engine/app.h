#pragma once

#include "engine/client.h"
#include "include/cef_app.h"
#include "include/cef_life_span_handler.h"

// CefApp + CefBrowserProcessHandler.
// Creates an offscreen browser when CEF context initializes.
class App : public CefApp, public CefBrowserProcessHandler {
 public:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
  void OnContextInitialized() override;

  CefRefPtr<Client> client() const { return client_; }

 private:
  CefRefPtr<Client> client_;
  IMPLEMENT_REFCOUNTING(App);
};
