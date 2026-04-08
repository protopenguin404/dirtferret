#include "engine/client.h"
#include <iostream>

Client::Client() : render_handler_(new Renderer()) {}

CefRefPtr<CefLifeSpanHandler> Client::GetLifeSpanHandler() { return this; }
CefRefPtr<CefRenderHandler> Client::GetRenderHandler() { return render_handler_; }
CefRefPtr<CefDisplayHandler> Client::GetDisplayHandler() { return this; }
CefRefPtr<CefLoadHandler> Client::GetLoadHandler() { return this; }

void Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    browser_ = browser;
    buffer_id_ = browser->GetIdentifier();
    std::cerr << "[cef] Browser created (id=" << buffer_id_ << ")." << std::endl;
    if (on_created_) {
        on_created_(this, buffer_id_);
    }
}

void Client::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    int32_t id = buffer_id_;
    browser_ = nullptr;
    buffer_id_ = -1;
    std::cerr << "[cef] Browser closed (id=" << id << ")." << std::endl;
    if (on_closed_) {
        on_closed_(id);
    }
}

void Client::OnTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title) {
    title_ = title.ToString();
    std::cerr << "[cef] Title [" << buffer_id_ << "]: " << title_ << std::endl;
    if (on_state_changed_) {
        on_state_changed_(buffer_id_);
    }
}

void Client::OnAddressChange(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             const CefString& url) {
    if (frame->IsMain()) {
        url_ = url.ToString();
        std::cerr << "[cef] URL [" << buffer_id_ << "]: " << url_ << std::endl;
        if (on_state_changed_) {
            on_state_changed_(buffer_id_);
        }
    }
}

void Client::OnLoadingProgressChange(CefRefPtr<CefBrowser> browser,
                                     double progress) {
    load_progress_ = progress;
    if (on_state_changed_) {
        on_state_changed_(buffer_id_);
    }
}

void Client::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                  bool isLoading,
                                  bool canGoBack,
                                  bool canGoForward) {
    is_loading_ = isLoading;
    can_go_back_ = canGoBack;
    can_go_forward_ = canGoForward;
    if (on_state_changed_) {
        on_state_changed_(buffer_id_);
    }
}
