#include "client.h"
#include "include/cef_app.h"
#include "include/cef_life_span_handler.h"

#include <iostream>

class MinimalApp : public CefApp, public CefBrowserProcessHandler {
public:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;

  void OnContextInitialized() override;

private:
  IMPLEMENT_REFCOUNTING(MinimalApp);
};
