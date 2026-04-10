#include "engine/app.h"
#include "include/cef_command_line.h"
#include <iostream>

CefRefPtr<CefBrowserProcessHandler> App::GetBrowserProcessHandler() {
    return this;
}

void App::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
    // Disable GPU — we use offscreen rendering (software) and the GPU process
    // crashes in Nix when libGL.so.1 is not on the library path.
    command_line->AppendSwitch("disable-gpu");
    command_line->AppendSwitch("disable-gpu-compositing");
}

void App::OnContextInitialized() {
    std::cerr << "[cef] Context initialized." << std::endl;
    if (ready_callback_) {
        ready_callback_();
    }
}
