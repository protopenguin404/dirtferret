
#include "app.h"
#include "include/cef_browser.h"

#include <iostream>

CefRefPtr<CefBrowserProcessHandler> MinimalApp::GetBrowserProcessHandler() {
  return this;
}

void MinimalApp::OnContextInitialized() {
  std::cerr << "[cef] Context initialized, creating offscreen browser."
            << std::endl;

  CefBrowserSettings browser_settings;
  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);

  client_ = new MinimalClient();

  CefBrowserHost::CreateBrowser(window_info, client_,
                                "https://example.com", browser_settings,
                                nullptr, nullptr);
}
