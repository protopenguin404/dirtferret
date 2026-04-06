#include "engine/client.h"

#include <iostream>

Client::Client() : render_handler_(new Renderer()) {}

CefRefPtr<CefLifeSpanHandler> Client::GetLifeSpanHandler() { return this; }
CefRefPtr<CefRenderHandler> Client::GetRenderHandler() { return render_handler_; }
CefRefPtr<CefDisplayHandler> Client::GetDisplayHandler() { return this; }
CefRefPtr<CefLoadHandler> Client::GetLoadHandler() { return this; }

void Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    browser_ = browser;
    std::cerr << "[cef] Browser created (id="
              << browser->GetIdentifier() << ")." << std::endl;
}

void Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    browser_ = nullptr;
    std::cerr << "[cef] Browser closed." << std::endl;
}

void Client::OnTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title) {
    title_ = title.ToString();
    std::cerr << "[cef] Title: " << title_ << std::endl;
}

void Client::OnAddressChange(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             const CefString& url) {
    if (frame->IsMain()) {
        url_ = url.ToString();
        std::cerr << "[cef] URL: " << url_ << std::endl;
    }
}

void Client::OnLoadingProgressChange(CefRefPtr<CefBrowser> browser,
                                     double progress) {
    load_progress_ = progress;
}

void Client::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                  bool isLoading,
                                  bool canGoBack,
                                  bool canGoForward) {
    is_loading_ = isLoading;
    can_go_back_ = canGoBack;
    can_go_forward_ = canGoForward;
}
