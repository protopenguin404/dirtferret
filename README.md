# CEF Quick Reference for Terminal Browser Development

This is a practical, no-fluff reference for using CEF (Chromium Embedded Framework) in an offscreen rendering context. It's written for someone building a terminal-based browser where CEF does the rendering and you handle everything else (display, input, navigation).

**What CEF actually is:** CEF wraps the Chromium browser engine into a C++ library. You embed it in your app. It does all the web stuff (HTML parsing, CSS layout, JavaScript execution, networking) and hands you pixel buffers. You decide what to do with those pixels.

**Official docs you should bookmark:**
- [CEF C++ API Reference](https://cef-builds.spotifycdn.com/docs/stable.html) -- the complete class/method reference
- [CEF Wiki on Bitbucket](https://bitbucket.org/chromiumembedded/cef/wiki/Home) -- conceptual guides
- [CEF Forum](https://magpcss.org/ceforum/) -- if you're stuck, someone has probably asked
- [GeneralUsage wiki page](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md) -- the single most important page, read this eventually

---

## Table of Contents

1. [How CEF Runs (The Process Model)](#1-how-cef-runs-the-process-model)
2. [The Class Hierarchy (What You Subclass)](#2-the-class-hierarchy-what-you-subclass)
3. [Offscreen Rendering (OSR)](#3-offscreen-rendering-osr)
4. [Navigation (Loading Pages)](#4-navigation-loading-pages)
5. [JavaScript Interaction](#5-javascript-interaction)
6. [Input (Keyboard and Mouse)](#6-input-keyboard-and-mouse)
7. [Browser Tabs / Multiple Browsers](#7-browser-tabs--multiple-browsers)
8. [Cookies, Cache, and Storage](#8-cookies-cache-and-storage)
9. [Downloads](#9-downloads)
10. [DevTools](#10-devtools)
11. [Error Handling and Logging](#11-error-handling-and-logging)
12. [Threading Model](#12-threading-model)
13. [Lifecycle and Shutdown](#13-lifecycle-and-shutdown)
14. [Common Pitfalls](#14-common-pitfalls)

---

## 1. How CEF Runs (The Process Model)

**The big picture:** When your app starts, CEF spawns *multiple OS processes*. This is not optional. Chromium does this for security and stability. Your `main()` function gets called multiple times -- once for the "browser" process (the main one you control) and once for each child process (renderer, GPU, network, etc.).

**What this looks like in code:**

```cpp
int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    CefRefPtr<MyApp> app(new MyApp());

    // THIS LINE is critical. It checks: "Am I a child process?"
    // If yes, it runs the child logic and returns an exit code >= 0.
    // If no (we're the browser process), it returns -1.
    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        return exit_code;  // We were a child. We're done.
    }

    // Only the browser process gets past here.
    CefSettings settings;
    CefInitialize(main_args, settings, app, nullptr);
    CefRunMessageLoop();
    CefShutdown();
    return 0;
}
```

**Why this matters to you:** Every time your binary runs, it might be a child process. Don't put terminal UI setup code before `CefExecuteProcess()` or child processes will try to grab your terminal. Everything that isn't CEF init should go *after* the `exit_code >= 0` check.

**Further reading:** [GeneralUsage > Processes](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md#markdown-header-processes)

---

## 2. The Class Hierarchy (What You Subclass)

CEF uses a "handler" pattern. You create classes that inherit from CEF base classes and override methods to respond to events. Here's what each one does and when you need it:

### CefApp (process-level setup)

You subclass this once. It gets called during process startup.

```cpp
class MyApp : public CefApp,
              public CefBrowserProcessHandler {
 public:
    // "Give me your browser-process handler" -- return `this` since we implement it
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    // Called once CEF is fully initialized. This is where you create your first browser.
    void OnContextInitialized() override {
        // Create browser here
    }

 private:
    IMPLEMENT_REFCOUNTING(MyApp);
};
```

**When you care:** Always. You always need a CefApp.

### CefClient (per-browser-instance handler hub)

This is the "router". When CEF needs to talk to your code about a specific browser instance, it asks the CefClient for the right handler. You return your custom handlers from here.

```cpp
class MyClient : public CefClient,
                 public CefLifeSpanHandler,
                 public CefLoadHandler,
                 public CefDisplayHandler,
                 public CefRequestHandler {
 public:
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefRenderHandler>   GetRenderHandler()   override { return render_handler_; }
    CefRefPtr<CefLoadHandler>     GetLoadHandler()     override { return this; }
    CefRefPtr<CefDisplayHandler>  GetDisplayHandler()  override { return this; }
    CefRefPtr<CefRequestHandler>  GetRequestHandler()  override { return this; }
    // ... more handlers as needed
};
```

**When you care:** Always. One CefClient per browser (or shared across browsers if you want).

### Handler classes you'll need (roughly in order of priority)

| Handler | What it does | When you need it |
|---|---|---|
| `CefRenderHandler` | Offscreen rendering -- gives you pixels | **Now.** You're in OSR mode. |
| `CefLifeSpanHandler` | Browser created/closed events | **Now.** Needed for clean shutdown. |
| `CefLoadHandler` | Page load started/finished/failed | Soon. For status bar, error pages. |
| `CefDisplayHandler` | Title changed, URL changed, console messages | Soon. For your status line. |
| `CefRequestHandler` | Before navigation, certificate errors, auth | When you want nav control. |
| `CefKeyboardHandler` | Key event filtering | When you build modal input. |
| `CefDownloadHandler` | File downloads | When you add download support. |
| `CefContextMenuHandler` | Right-click menus | Probably never (terminal). Suppress default by overriding. |
| `CefFocusHandler` | Focus changes | Maybe. Depends on multi-tab design. |

**The pattern is always the same:**
1. Your CefClient inherits from the handler (or holds a separate handler object)
2. You override the `Get*Handler()` method on CefClient to return it
3. You override the callback methods on the handler to do your thing

**Further reading:** [CefClient API docs](https://cef-builds.spotifycdn.com/docs/stable.html?classCefClient.html)

---

## 3. Offscreen Rendering (OSR)

This is the core of your project. In OSR mode, CEF never creates a visible window. Instead, it renders web pages into memory buffers and calls your `OnPaint()` method with the pixel data.

### Setup (what you already have)

```cpp
// When creating the browser, tell CEF it's windowless:
CefWindowInfo window_info;
window_info.SetAsWindowless(0);  // 0 = no parent window handle

// In CefSettings:
settings.windowless_rendering_enabled = true;
```

### CefRenderHandler -- the important methods

```cpp
class MyRenderHandler : public CefRenderHandler {
 public:
    // CEF asks: "How big is the screen?"
    // You answer with your terminal's pixel dimensions.
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        // Example: 80 cols x 24 rows, each cell 8x16 pixels = 640x384
        rect = CefRect(0, 0, pixel_width_, pixel_height_);
    }

    // CEF says: "Here are the pixels that changed."
    // `buffer` is BGRA format, 4 bytes per pixel, row-major.
    // `width` and `height` are the full viewport size.
    // `dirtyRects` tells you which rectangles actually changed (optimization).
    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,      // PET_VIEW or PET_POPUP
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width,
                 int height) override {
        // type == PET_VIEW means the main page content
        // type == PET_POPUP means dropdown menus, autocomplete, etc.

        // buffer layout:
        //   - Total size: width * height * 4 bytes
        //   - Pixel at (x, y): buffer[(y * width + x) * 4]
        //   - Byte order: B, G, R, A (Blue, Green, Red, Alpha)

        // Your job: convert this to kitty graphics protocol and send to terminal
    }

    // CEF asks: "Do you want to get screen info?"
    // Used for DPI scaling. Return true and fill in `screen_info`.
    bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                       CefScreenInfo& screen_info) override {
        screen_info.device_scale_factor = 1.0;  // 1.0 = no scaling
        screen_info.rect = CefRect(0, 0, pixel_width_, pixel_height_);
        screen_info.available_rect = screen_info.rect;
        return true;
    }

    // Called when the cursor should change (arrow, hand, text, etc.)
    // In a terminal you probably ignore this, but you could map to terminal cursor shapes.
    bool OnCursorChange(CefRefPtr<CefBrowser> browser, ...) override {
        return false;  // We don't handle cursor changes
    }
};
```

### Forcing a repaint

Sometimes you need CEF to repaint (e.g., after resize). Call:

```cpp
browser->GetHost()->Invalidate(PET_VIEW);  // Triggers OnPaint
```

### Changing the viewport size

When the terminal resizes, you need to:
1. Update your stored `pixel_width_` / `pixel_height_`
2. Call `browser->GetHost()->WasResized()` -- this makes CEF call `GetViewRect()` again and repaint

### Frame rate

```cpp
// In CefBrowserSettings, before creating the browser:
CefBrowserSettings browser_settings;
browser_settings.windowless_frame_rate = 30;  // Default is 30 FPS. Max is 60.
```

**Important:** `OnPaint()` is called on the **UI thread**. Don't do slow work in it. Copy the buffer and process it elsewhere if needed.

**Further reading:** [CefRenderHandler API](https://cef-builds.spotifycdn.com/docs/stable.html?classCefRenderHandler.html)

---

## 4. Navigation (Loading Pages)

### Load a URL

```cpp
browser->GetMainFrame()->LoadURL("https://example.com");
```

That's it. Seriously. CEF handles DNS, TLS, redirects, everything.

### Go back / forward

```cpp
if (browser->CanGoBack())    browser->GoBack();
if (browser->CanGoForward()) browser->GoForward();
```

### Reload

```cpp
browser->Reload();                // Normal reload
browser->ReloadIgnoreCache();     // Hard reload (Ctrl+Shift+R equivalent)
```

### Stop loading

```cpp
browser->StopLoad();
```

### Get current URL and title

```cpp
std::string url = browser->GetMainFrame()->GetURL();
```

For the title, use `CefDisplayHandler::OnTitleChange()`:

```cpp
void OnTitleChange(CefRefPtr<CefBrowser> browser,
                   const CefString& title) override {
    std::string page_title = title.ToString();
    // Update your terminal status bar
}
```

### Know when a page finishes loading

Use `CefLoadHandler`:

```cpp
void OnLoadStart(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 TransitionType transition_type) override {
    if (frame->IsMain()) {
        // Main page started loading -- show spinner?
    }
}

void OnLoadEnd(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int httpStatusCode) override {
    if (frame->IsMain()) {
        // Main page done loading. httpStatusCode is the HTTP status (200, 404, etc.)
    }
}

void OnLoadError(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 ErrorCode errorCode,
                 const CefString& errorText,
                 const CefString& failedUrl) override {
    // Network error, DNS failure, etc.
    // errorCode is a Chromium net error (ERR_NAME_NOT_RESOLVED, etc.)
}
```

**Further reading:** [CefBrowser API](https://cef-builds.spotifycdn.com/docs/stable.html?classCefBrowser.html), [CefFrame API](https://cef-builds.spotifycdn.com/docs/stable.html?classCefFrame.html)

---

## 5. JavaScript Interaction

### Execute JS in a page (fire-and-forget)

```cpp
browser->GetMainFrame()->ExecuteJavaScript(
    "document.title",      // JavaScript code
    "about:blank",         // URL for error reporting (can be anything)
    0                      // Line number for error reporting
);
```

**Limitation:** `ExecuteJavaScript()` is fire-and-forget. You can't get a return value from it directly.

### Get a value back from JS

This is more involved. You need to use **CefV8** in the **renderer process** (not the browser process). The short version:

1. Create a `CefRenderProcessHandler` and register it in your `CefApp::GetRenderProcessHandler()`
2. Use **process messaging** (`CefProcessMessage`) to send requests from browser -> renderer and responses back

This is one of the more complex parts of CEF. For your terminal browser, you'll likely need this for:
- Getting page scroll position
- Extracting text content
- Querying DOM state

**Simpler alternative for many cases:** Use `ExecuteJavaScript()` to run JS that modifies the page in a way you can observe (e.g., writing to the title, which triggers `OnTitleChange`).

**Further reading:** [GeneralUsage > JavaScript Integration](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md#markdown-header-javascript-integration)

---

## 6. Input (Keyboard and Mouse)

Since you're in OSR mode, CEF doesn't get input from anywhere automatically. You have to synthesize input events and send them to CEF. This is how your terminal keyboard/mouse input becomes browser interaction.

### Keyboard

```cpp
CefKeyEvent key_event;

// For a regular character key (e.g., typing 'a'):
key_event.type = KEYEVENT_CHAR;
key_event.character = 'a';
key_event.unmodified_character = 'a';
key_event.windows_key_code = 'A';  // Yes, uppercase for the keycode
key_event.native_key_code = 0;     // X11 keycode, 0 is fine for most cases
key_event.modifiers = 0;           // EVENTFLAG_SHIFT_DOWN, EVENTFLAG_CONTROL_DOWN, etc.

browser->GetHost()->SendKeyEvent(key_event);

// For special keys, you need KEYEVENT_RAWKEYDOWN + KEYEVENT_KEYUP:
CefKeyEvent enter_down;
enter_down.type = KEYEVENT_RAWKEYDOWN;
enter_down.windows_key_code = 13;   // VK_RETURN
enter_down.native_key_code = 36;    // X11 keycode for Enter
browser->GetHost()->SendKeyEvent(enter_down);

CefKeyEvent enter_up;
enter_up.type = KEYEVENT_KEYUP;
enter_up.windows_key_code = 13;
enter_up.native_key_code = 36;
browser->GetHost()->SendKeyEvent(enter_up);
```

**Modifier flags you'll use:**

| Flag | Meaning |
|---|---|
| `EVENTFLAG_SHIFT_DOWN` | Shift held |
| `EVENTFLAG_CONTROL_DOWN` | Ctrl held |
| `EVENTFLAG_ALT_DOWN` | Alt held |
| `EVENTFLAG_LEFT_MOUSE_BUTTON` | Left mouse button held (for drag) |

### Mouse

```cpp
CefMouseEvent mouse_event;
mouse_event.x = 100;  // Pixel coordinates relative to the viewport
mouse_event.y = 200;
mouse_event.modifiers = 0;

// Move the mouse (hover):
browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
// The `false` means "mouse is inside the view". Use `true` for "mouse left the view".

// Click:
browser->GetHost()->SendMouseClickEvent(
    mouse_event,
    MBT_LEFT,     // MBT_LEFT, MBT_RIGHT, or MBT_MIDDLE
    false,        // false = mouse down, true = mouse up
    1             // click count (1 = single click, 2 = double click)
);
// You need to send both down AND up for a complete click:
browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT, true, 1);

// Scroll:
browser->GetHost()->SendMouseWheelEvent(
    mouse_event,
    0,     // deltaX (horizontal scroll)
    -120   // deltaY (negative = scroll down, positive = scroll up)
           // 120 = one "notch" of a typical mouse wheel
);
```

**For your terminal:** You'll map terminal mouse events (if supported by the terminal) to these pixel coordinates. You'll need to know the pixel-per-cell ratio of your terminal to convert cell coordinates to pixel coordinates.

**Further reading:** [CefBrowserHost API (input methods)](https://cef-builds.spotifycdn.com/docs/stable.html?classCefBrowserHost.html)

---

## 7. Browser Tabs / Multiple Browsers

CEF calls them "browsers", not "tabs". Each browser is an independent web page. For your nvim-like buffer model:

### Create a new browser (new "tab")

```cpp
CefWindowInfo window_info;
window_info.SetAsWindowless(0);

CefBrowserSettings settings;
CefBrowserHost::CreateBrowser(
    window_info,
    your_client,          // Can be the same CefClient for all, or separate ones
    "https://example.com",
    settings,
    nullptr,              // CefDictionaryValue* extra_info
    nullptr               // CefRequestContext* (nullptr = share global context)
);
```

`CreateBrowser` is **asynchronous**. The browser isn't ready immediately. You get notified via `CefLifeSpanHandler::OnAfterCreated()`.

### Track your browsers

```cpp
// In your CefLifeSpanHandler:
void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    int id = browser->GetIdentifier();  // Unique ID for this browser
    browsers_[id] = browser;            // Store in your map/vector
}

void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    browsers_.erase(browser->GetIdentifier());
    if (browsers_.empty()) {
        CefQuitMessageLoop();  // Last browser closed, shut down
    }
}
```

### Close a browser

```cpp
// Polite close (lets the page run beforeunload handlers):
browser->GetHost()->CloseBrowser(false);

// Force close (skips beforeunload):
browser->GetHost()->CloseBrowser(true);
```

### Switch which browser is "active"

CEF doesn't have a concept of "active tab". That's entirely your UI's job. You just decide which browser's `OnPaint` output you display and which browser gets your input events.

```cpp
// Send input to the active browser only:
active_browser_->GetHost()->SendKeyEvent(key_event);

// In OnPaint, check if this is the active browser before rendering:
void OnPaint(CefRefPtr<CefBrowser> browser, ...) override {
    if (browser->GetIdentifier() == active_browser_id_) {
        // Render this to the terminal
    }
    // Inactive browsers: their OnPaint still fires (they're still rendering
    // in the background) but you just ignore the pixels.
}
```

---

## 8. Cookies, Cache, and Storage

### Request context (session management)

By default, all browsers share a global `CefRequestContext`. This means shared cookies, cache, and storage. Fine for most cases.

```cpp
// Create an isolated context (like incognito):
CefRequestContextSettings rc_settings;
CefString(&rc_settings.cache_path).FromASCII("/tmp/cef-isolated-session");
auto request_context = CefRequestContext::CreateContext(rc_settings, nullptr);

// Pass it when creating the browser:
CefBrowserHost::CreateBrowser(window_info, client, url, settings, nullptr, request_context);
```

### Set the cache path (persistent cookies/storage)

```cpp
CefSettings settings;
CefString(&settings.cache_path).FromASCII("/home/user/.cache/cef-terminal");
// If you don't set this, everything is in-memory and lost on exit.
```

### Cookie access

```cpp
auto manager = CefCookieManager::GetGlobalManager(nullptr);

// Visit (iterate) all cookies:
class MyCookieVisitor : public CefCookieVisitor {
    bool Visit(const CefCookie& cookie, int count, int total,
               bool& deleteCookie) override {
        // cookie.name, cookie.value, cookie.domain, etc.
        deleteCookie = false;  // Set to true to delete this cookie
        return true;  // Continue visiting
    }
    IMPLEMENT_REFCOUNTING(MyCookieVisitor);
};
manager->VisitAllCookies(new MyCookieVisitor());
```

---

## 9. Downloads

Implement `CefDownloadHandler`:

```cpp
class MyClient : public CefClient, public CefDownloadHandler {
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }

    // Called before a download starts. Decide where to save it.
    void OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefDownloadItem> download_item,
                          const CefString& suggested_name,
                          CefRefPtr<CefBeforeDownloadCallback> callback) override {
        // To accept the download and save to a specific path:
        callback->Continue("/home/user/Downloads/" + suggested_name.ToString(), false);
        // The `false` means "don't show a save dialog" (there is no dialog in OSR anyway)
    }

    // Called periodically with progress updates.
    void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefDownloadItem> download_item,
                           CefRefPtr<CefDownloadItemCallback> callback) override {
        if (download_item->IsComplete()) {
            // Done!
        }
        // download_item->GetPercentComplete(), GetReceivedBytes(), etc.
    }
};
```

---

## 10. DevTools

Useful for debugging. CEF can serve a DevTools interface over HTTP:

```cpp
CefWindowInfo devtools_info;
devtools_info.SetAsWindowless(0);  // Or a real window if you have one

// Open DevTools as a separate browser:
browser->GetHost()->ShowDevTools(devtools_info, new DevToolsClient(), CefBrowserSettings(), CefPoint());

// OR: just enable remote debugging on a port (easier for terminal use):
// In CefSettings, before CefInitialize:
settings.remote_debugging_port = 9222;
// Then open http://localhost:9222 in another browser to inspect your pages.
```

**For development:** The `remote_debugging_port` approach is probably most useful. You can inspect your rendered pages in a regular browser while working on the terminal rendering.

---

## 11. Error Handling and Logging

### CEF's built-in logging

```cpp
CefSettings settings;
settings.log_severity = LOGSEVERITY_WARNING;  // VERBOSE, INFO, WARNING, ERROR, FATAL, DISABLE
CefString(&settings.log_file).FromASCII("/tmp/cef-terminal.log");
```

Log levels (from most to least verbose): `VERBOSE` > `INFO` > `WARNING` > `ERROR` > `FATAL` > `DISABLE`

### Console messages from web pages

When JavaScript on a page calls `console.log()`, you get it via `CefDisplayHandler`:

```cpp
bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                      cef_log_severity_t level,
                      const CefString& message,
                      const CefString& source,
                      int line) override {
    std::cerr << "[JS " << source.ToString() << ":" << line << "] "
              << message.ToString() << std::endl;
    return true;  // true = we handled it, don't log to CEF's default log
}
```

---

## 12. Threading Model

**This is important. Read this even if you skip other sections.**

CEF uses multiple threads. The main ones you'll interact with:

| Thread | Name constant | What runs here |
|---|---|---|
| UI thread | `TID_UI` | Browser process main thread. Message loop, most callbacks. |
| IO thread | `TID_IO` | Network requests, IPC with child processes. |
| FILE thread | `TID_FILE_BACKGROUND` | Slow file operations. |
| RENDERER thread | (separate process) | JavaScript execution, DOM access. Different process entirely. |

**The rules:**
1. Most CEF callbacks happen on the **UI thread**. Don't block it. If you do slow work, post it to another thread.
2. Most CEF API calls must be made from the **UI thread**. If you're on another thread, post a task.
3. Your terminal input loop will need to coordinate with the CEF UI thread.

### Checking which thread you're on

```cpp
#include "include/cef_task.h"

if (CefCurrentlyOn(TID_UI)) {
    // Safe to call CEF APIs
} else {
    // Need to post to UI thread
}
```

### Posting work to the UI thread

```cpp
#include "include/cef_task.h"

// Option 1: Lambda (C++14+, cleanest)
CefPostTask(TID_UI, base::BindLambda([browser]() {
    browser->GetMainFrame()->LoadURL("https://example.com");
}));

// Option 2: CefTask subclass (more boilerplate, works everywhere)
class MyTask : public CefTask {
 public:
    void Execute() override {
        // Do stuff on the UI thread
    }
    IMPLEMENT_REFCOUNTING(MyTask);
};
CefPostTask(TID_UI, new MyTask());
```

### The message loop problem (relevant to you)

Right now you have `CefRunMessageLoop()` which blocks forever. For a terminal UI, you need to also process terminal input. Two options:

**Option A: `CefDoMessageLoopWork()` (recommended for your project)**
Call this periodically from your own event loop. CEF processes its pending work and returns.

```cpp
// Your main loop:
while (running) {
    CefDoMessageLoopWork();          // Let CEF do its thing
    process_terminal_input();         // Read from stdin, handle keys
    update_terminal_display();        // Push pixels to terminal
    std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps cap
}
```

**Option B: `CefSettings::multi_threaded_message_loop = true`**
CEF runs its message loop on a separate thread. Your main thread is free. But this means you must be careful about thread safety when calling CEF APIs from your terminal input thread (always post to `TID_UI`).

**Further reading:** [GeneralUsage > Threads](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md#markdown-header-threads)

---

## 13. Lifecycle and Shutdown

CEF is very particular about shutdown order. Getting this wrong causes crashes or hangs.

### The correct shutdown sequence

1. Close all browsers: `browser->GetHost()->CloseBrowser(true)` for each one
2. Wait for all `OnBeforeClose()` callbacks to fire (one per browser)
3. After the last browser closes, call `CefQuitMessageLoop()` (which you're already doing)
4. After the message loop exits, call `CefShutdown()`
5. Then exit your process

### What goes wrong if you don't do this

- If you call `CefShutdown()` while browsers are still open: **crash**
- If you just `exit()` without `CefShutdown()`: **resource leaks, orphan child processes**
- If you call `CefShutdown()` from the wrong thread: **hang**

### Force-quitting cleanly

If you need to bail out (user presses Ctrl+C or equivalent):

```cpp
// Close all browsers forcefully:
for (auto& [id, browser] : browsers_) {
    browser->GetHost()->CloseBrowser(true);
}
// OnBeforeClose will fire for each, and when the last one fires,
// CefQuitMessageLoop() gets called, which unblocks CefRunMessageLoop(),
// which lets you reach CefShutdown().
```

---

## 14. Common Pitfalls

These are things that will bite you. Ordered by how likely they are to waste your time:

### "My binary crashes on startup"
- Almost always: CEF can't find its `.so` files, `.pak` resources, or `locales/` directory. They must be in the same directory as the binary (or specified via `CefSettings`). Our build system handles this via `COPY_FILES()` in CMake.

### "OnPaint is never called"
- Did you set `window_info.SetAsWindowless(0)`?
- Did you set `settings.windowless_rendering_enabled = true`?
- Does `GetViewRect()` return a non-zero size?
- Is the browser actually loading a page?

### "Segfault in CefShutdown"
- You probably have a `CefRefPtr` to a CEF object that outlives `CefShutdown()`. Make sure all your CefRefPtr members are cleared/destructed before shutdown.

### "My app hangs on exit"
- A browser is still open. Check that all `OnBeforeClose()` callbacks fired.

### "Text input doesn't work"
- OSR keyboard input needs all three event types for typed characters: `KEYEVENT_RAWKEYDOWN`, `KEYEVENT_CHAR`, `KEYEVENT_KEYUP`. Missing any one can cause issues.

### "Scrolling doesn't work"
- `SendMouseWheelEvent` deltaY must be non-zero. Typical values: +/- 120.

### Memory management (CefRefPtr)
- CEF uses reference counting, not `std::shared_ptr`. `CefRefPtr<T>` is CEF's smart pointer.
- You **must** include `IMPLEMENT_REFCOUNTING(ClassName)` in every class that inherits from a CEF base class.
- Don't use `new` and store in a raw pointer. Always use `CefRefPtr`.
- Don't call `delete` on CefRefPtr-managed objects. The ref count handles it.

```cpp
// Correct:
CefRefPtr<MyClient> client(new MyClient());

// Also correct (C++11):
CefRefPtr<MyClient> client = new MyClient();

// Wrong -- raw pointer, will leak or double-free:
MyClient* client = new MyClient();
```

---

## Appendix: Kitty Graphics Protocol (for your render pipeline)

Since this project sends CEF's pixel output to a terminal via the kitty graphics protocol, here's the relevant info:

**What it is:** A terminal escape sequence protocol that lets you send raster images to the terminal. The terminal displays them inline, overlaid on the text grid.

**Basic format:**
```
ESC_]G<control data>;<payload>ESC_\
```
Where the payload is base64-encoded pixel data.

**For sending a full frame:**
```
\033_Ga=T,f=32,s=<width>,v=<height>,m=1;<base64 chunk>\033\\
\033_Gm=1;<base64 chunk>\033\\
...
\033_Gm=0;<base64 chunk>\033\\
```
- `a=T` = transmit and display
- `f=32` = RGBA format (32 bits per pixel). CEF gives you BGRA, so you'll need to swap R and B channels.
- `s=<width>` = pixel width
- `v=<height>` = pixel height
- `m=1` = more chunks coming, `m=0` = last chunk

**Docs:** https://sw.kovidgoyal.net/kitty/graphics-protocol/

---

*This reference will grow as the project develops. When in doubt, check the [CEF C++ API docs](https://cef-builds.spotifycdn.com/docs/stable.html).*
