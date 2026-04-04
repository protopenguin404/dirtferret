#include "client.h"
#include "include/cef_app.h"
#include "include/cef_life_span_handler.h"

#include <iostream>

class MinimalApp : public CefApp, public CefBrowserProcessHandler {
public:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;

  void OnContextInitialized() override;

  // Access the client (and through it, the browser and title).
  CefRefPtr<MinimalClient> client() const { return client_; }

private:
  CefRefPtr<MinimalClient> client_;
  IMPLEMENT_REFCOUNTING(MinimalApp);
};
