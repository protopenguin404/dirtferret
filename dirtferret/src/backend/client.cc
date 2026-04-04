
#include "client.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"

#include <iostream>

MinimalClient::MinimalClient() : render_handler_(new RenderHandler()) {}

CefRefPtr<CefLifeSpanHandler> MinimalClient::GetLifeSpanHandler() {
  return this;
}

CefRefPtr<CefRenderHandler> MinimalClient::GetRenderHandler() {
  return render_handler_;
}

CefRefPtr<CefDisplayHandler> MinimalClient::GetDisplayHandler() {
  return this;
}

void MinimalClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  browser_ = browser;
  std::cerr << "[cef] Browser created." << std::endl;
}

void MinimalClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  browser_ = nullptr;
  std::cerr << "[cef] Browser closed." << std::endl;
}

void MinimalClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  title_ = title.ToString();
  std::cerr << "[cef] Title changed: " << title_ << std::endl;
}
