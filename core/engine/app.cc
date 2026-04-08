#include "engine/app.h"
#include <iostream>

CefRefPtr<CefBrowserProcessHandler> App::GetBrowserProcessHandler() {
    return this;
}

void App::OnContextInitialized() {
    std::cerr << "[cef] Context initialized." << std::endl;
    if (ready_callback_) {
        ready_callback_();
    }
}
