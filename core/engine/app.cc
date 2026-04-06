#include "engine/app.h"
#include "include/cef_browser.h"

#include <iostream>

CefRefPtr<CefBrowserProcessHandler> App::GetBrowserProcessHandler() {
    return this;
}

void App::OnContextInitialized() {
    std::cerr << "[cef] Context initialized, creating offscreen browser."
              << std::endl;

    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 30;

    CefWindowInfo window_info;
    window_info.SetAsWindowless(0);

    client_ = new Client();

    CefBrowserHost::CreateBrowser(window_info, client_,
                                  "about:blank", browser_settings,
                                  nullptr, nullptr);
}
