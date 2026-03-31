
#include "client.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_render_handler.h"

#include <iostream>

// Minimal client that wires up offscreen rendering and browser lifetime.
MinimalClient::MinimalClient() : render_handler_(new RenderHandler()) {}

CefRefPtr<CefLifeSpanHandler> MinimalClient::GetLifeSpanHandler() {
  return this;
}
CefRefPtr<CefRenderHandler> MinimalClient::GetRenderHandler() {
  return render_handler_;
}

void MinimalClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  std::cerr << "[cef] Browser created." << std::endl;
}

void MinimalClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  std::cerr << "[cef] Browser closed, quitting message loop." << std::endl;
  CefQuitMessageLoop();
}
