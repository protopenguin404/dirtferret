# Architecture

dirtferret is a terminal-based web browser built on CEF (Chromium Embedded Framework). CEF runs as a headless renderer producing raw pixel buffers. A Rust terminal client displays them via the kitty graphics protocol. The result is a full Chromium browser inside your terminal.

The architecture follows Neovim's model: a headless core process exposes a typed RPC interface. UIs, plugins, and external tools all consume the same API. The core is a programmable browser kernel; the terminal client is just one possible frontend.

## Design Principles

**Neovim's kernel model.** The core is headless and UI-agnostic. All state lives in the core. UIs are stateless renderers that receive events and send input. Multiple UIs can connect simultaneously (detach/reattach, multiple terminals, external tools).

**Schema-driven API.** [Cap'n Proto](https://capnproto.org/) defines the entire API surface as `.capnp` schema files. The schema is the single source of truth for what the core offers. From it, we generate C++ server code, Rust client code, and Lua bindings. Introspection is built in — any client can discover the full API at runtime by reading the schema.

**Zero-copy frame delivery.** Pixel data from CEF's `OnPaint` goes into shared memory. Cap'n Proto RPC carries only the metadata (dimensions, dirty rectangles, shm reference). The terminal client reads pixels directly from shared memory and sends them to the terminal via the kitty graphics protocol's shared memory mode (`t=s`). Pixels are never serialized or copied through the RPC layer.

**Composition over inheritance.** Outside of CEF's required handler subclassing, prefer interfaces, callbacks, and dependency injection. Design every component for composability.

**Policy in plugins, mechanics in the core.** If the user might want to configure it — keybindings, default URLs, tab behavior, ad blocking rules — it is policy and belongs in a Lua plugin. If it is internal coordination or timing, it is mechanical and belongs in the core.

## Process Model

```
dirtferret-core (C++ daemon)
  |
  |--- CEF child processes (renderer, GPU, utility — managed by CEF internally)
  |
  |=== Cap'n Proto RPC (Unix socket) ===> dirtferret-tui (Rust, terminal 1)
  |=== Cap'n Proto RPC (Unix socket) ===> dirtferret-tui (Rust, terminal 2)
  |=== Cap'n Proto RPC (Unix socket) ===> external tool / remote plugin
```

**dirtferret-core** is a long-running daemon. It owns all browser state: CEF browser instances (buffers), navigation history, cookies, extensions, the Lua plugin runtime. It runs CEF in offscreen rendering mode — no X11 windows, no GTK. It exposes a Cap'n Proto RPC server over a Unix domain socket.

**dirtferret-tui** is a lightweight Rust terminal client built with [ratatui](https://ratatui.rs/). It connects to the core via Cap'n Proto RPC, renders pixels via the kitty graphics protocol, captures keyboard and mouse input, and manages terminal chrome (tab bar, status line, command bar). Multiple TUI instances can connect to the same core (shared session, like tmux).

**External tools** connect to the same RPC socket. A Python script, a Rust CLI, a Lua debugger — anything with a Cap'n Proto client library can query buffers, send commands, or subscribe to events. This is the equivalent of Neovim's `--listen` socket.

### Startup Sequence

Following Neovim's `--embed` pattern:

1. Core starts, initializes CEF, binds the RPC socket
2. Core waits for a UI to call `attachUi()` before sourcing user config
3. TUI connects, sends `attachUi(width, height)` with a `Ui` callback capability
4. Core sources `init.lua`, fires the `UiAttach` event, begins frame delivery
5. TUI enters its render loop

This ordering lets the UI handle early messages (errors during config loading, permission prompts) rather than losing them to a headless start.

## API Design

The API is defined entirely in Cap'n Proto schema files under `schema/`. Cap'n Proto's RPC system provides:

- **Typed interfaces** — methods with named, typed parameters and return values
- **Promise pipelining** — chain calls without waiting for round trips
- **Bidirectional RPC** — both sides can call each other (core calls UI callbacks)
- **Built-in introspection** — schema metadata is embeddable and queryable

### Core Interface

The core exposes a `Core` RPC interface. This is the browser kernel's API, analogous to Neovim's `nvim_*` functions.

```capnp
interface Core {
  createBuffer  @0 (url :Text) -> (bufferId :Int32);
  closeBuffer   @1 (bufferId :Int32) -> ();
  listBuffers   @2 () -> (buffers :List(BufferInfo));
  getBufferInfo @3 (bufferId :Int32) -> (info :BufferInfo);
  navigate      @4 (bufferId :Int32, url :Text) -> ();
  goBack        @5 (bufferId :Int32) -> ();
  goForward     @6 (bufferId :Int32) -> ();
  reload        @7 (bufferId :Int32) -> ();
  stopLoad      @8 (bufferId :Int32) -> ();
  # ... input, zoom, find, JS execution, UI attachment
  attachUi     @20 (ui :Ui, width :UInt32, height :UInt32) -> ();
}
```

### UI Callback Interface

When a TUI calls `attachUi()`, it passes a `Ui` capability — an RPC interface that the core calls back into. This is analogous to Neovim's `redraw` notification stream.

```capnp
interface Ui {
  onFrame               @0 (bufferId :Int32, shmName :Text,
                            width :UInt32, height :UInt32,
                            format :PixelFormat,
                            dirtyRects :List(Rect)) -> ();
  onTitleChanged        @3 (bufferId :Int32, title :Text) -> ();
  onUrlChanged          @4 (bufferId :Int32, url :Text) -> ();
  onLoadingStateChanged @5 (bufferId :Int32, loading :Bool,
                            canGoBack :Bool, canGoForward :Bool) -> ();
  onFocusedFieldChanged @7 (bufferId :Int32, editable :Bool) -> ();
  # ... more state update callbacks
}
```

The `onFocusedFieldChanged` callback is critical for modal input: when `editable` becomes true, the TUI can auto-switch to insert mode (passing keystrokes to CEF); when false, it can return to normal mode (keystrokes handled locally).

## Frame Delivery Pipeline

This is the critical data path. Pixel data flows from CEF to the terminal without serialization.

```
CEF OnPaint(buffer, width, height, dirtyRects)
  |
  v
Core: memcpy into shared memory segment (/dev/shm/dirtferret-frame-N)
  |
  v
Core: RPC call to Ui.onFrame(shmName, width, height, dirtyRects)
  |                                    (metadata only, no pixels in the message)
  v
TUI: mmap shared memory, read pixel data
  |
  v
TUI: BGRA -> RGBA conversion (swap B and R channels)
  |
  v
TUI: write kitty graphics escape sequence
     ESC _Ga=T,f=32,s=W,v=H,t=s;/dev/shm/dirtferret-frame-N ESC \
  |
  v
Terminal (kitty/WezTerm/Ghostty): renders pixels
```

**Key details:**

- CEF's `OnPaint` delivers BGRA pixel data with a list of dirty rectangles
- The core writes pixels into a POSIX shared memory segment (`shm_open` / `memfd_create`)
- Only metadata travels through Cap'n Proto RPC — the pixels stay in shared memory
- The kitty graphics protocol supports shared memory mode (`t=s`), allowing the terminal to read directly from the same shm segment — true zero-copy from CEF to display
- Double-buffering: two shm segments alternate so the core can write the next frame while the TUI reads the current one
- Dirty rectangle tracking: only changed regions need BGRA-to-RGBA conversion and kitty protocol update

**Frame rate:** CEF's `windowless_frame_rate` setting controls how often `OnPaint` fires (1-60 fps, default 30). The core can also use `SendExternalBeginFrame()` for client-driven frame timing.

**Inactive buffers:** `CefBrowserHost::WasHidden(true)` stops `OnPaint` for hidden buffers (tabs not currently displayed), saving GPU and bandwidth.

## Buffer / Window / Tab Model

Following Neovim's triad:

| Concept | Neovim | dirtferret | Implementation |
|---------|--------|------------|----------------|
| **Buffer** | File contents + metadata | One CEF browser instance + metadata | `CefBrowser` with integer ID from `GetIdentifier()` |
| **Window** | Viewport into a buffer | Viewport region in the terminal | ratatui `Rect` with kitty graphics placement |
| **Tab page** | Collection of windows | A saved window arrangement | TUI-side layout state |

- A buffer can exist without being displayed (background tab)
- Multiple windows can show the same buffer (split view)
- Switching tab pages changes the entire window layout
- Buffer IDs are CEF's `CefBrowser::GetIdentifier()` — also used as Chrome extension `tabId`

## Scripting

### Lua Runtime

The core embeds LuaJIT as its primary scripting engine, following Neovim's model. Lua plugins have direct, zero-overhead access to the core API — no RPC serialization.

**Binding generation:** A build-time code generator reads the `.capnp` schema and produces Lua-to-C++ bridge functions. The result is a `dirtferret` table in Lua:

```lua
local buf = dirtferret.buffer.create({ url = "https://example.com" })
local info = dirtferret.buffer.get_info(buf)
print(info.title)

dirtferret.buffer.navigate(buf, "https://github.com")

dirtferret.on("TitleChanged", function(event)
    print("Title is now: " .. event.title)
end)

dirtferret.keymap.set("n", "gd", function()
    dirtferret.buffer.navigate(dirtferret.buffer.active(), "https://duckduckgo.com")
end)
```

This is analogous to `vim.api.*` in Neovim. The Lua functions call directly into C++ — same thread, no serialization. The `.capnp` schema ensures the Lua API surface matches what external RPC clients see.

### External Plugins (RPC)

Any language with a Cap'n Proto library can connect to the core's RPC socket and act as a plugin:

```python
import capnp
core = capnp.load("schema/core.capnp")
client = core.Core.connect("unix:/tmp/dirtferret.sock")

buffers = client.listBuffers().wait()
for buf in buffers.buffers:
    print(f"{buf.id}: {buf.title} — {buf.url}")
```

## Extension Support

Chrome extension compatibility is a long-term goal, not a launch requirement.

### Launch Target: Native Equivalents

The most-requested extension functionality is implemented natively:

- **Ad/tracker blocking:** `CefResourceRequestHandler` intercepts network requests. Lua plugins provide filter lists and rules (equivalent to `declarativeNetRequest`).
- **Content injection:** `ExecuteJavaScript` injects user scripts into pages (equivalent to content scripts). Lua plugins manage injection rules.
- **Custom CSS:** Dark mode, readability — injected via JavaScript.

This covers the majority of real-world extension use: ad blockers, dark mode, user scripts, privacy tools.

### Future: Chrome Extension Loading

When CEF adds programmatic extension management APIs ([#3450](https://github.com/chromiumembedded/cef/issues/3450)), extensions can be loaded from a profile directory and their state exposed through the RPC API.

### Architectural Hooks

Even before extensions are functional, the architecture includes hooks for future support:

- **Buffer IDs = `CefBrowser::GetIdentifier()`** — already matches what extensions expect for `chrome.tabs` `tabId`
- **`CefRequestContext`** — per-context extension isolation is built into CEF
- **`CefRequestHandler` / `CefResourceRequestHandler`** — provides the same request interception that `chrome.webRequest` uses

## Technology Stack

| Component | Technology | Role |
|-----------|-----------|------|
| **Browser engine** | CEF 142 (Chromium 142) | Page rendering, JavaScript, networking |
| **Core language** | C++17 | CEF integration, Lua hosting, RPC server |
| **Frontend language** | Rust | Terminal UI, input handling, kitty graphics |
| **Terminal UI** | ratatui + crossterm | Layout, widgets, event loop |
| **RPC** | Cap'n Proto | API definition, cross-language communication |
| **Scripting** | LuaJIT | User configuration, plugins, policy logic |
| **Graphics** | Kitty graphics protocol | Pixel display in terminal |
| **Build** | CMake + Cargo + Nix | Compilation, codegen, environment |

## Directory Structure

```
schema/              Cap'n Proto schemas (source of truth for API)
  types.capnp        Shared types (BufferInfo, KeyEvent, Rect, enums)
  core.capnp         Core + Ui interfaces

core/                C++ backend (browser kernel)
  main.cc            Entry point
  engine/            CEF integration (app, client, renderer, engine)
  shm/               Shared memory frame pool
  CMakeLists.txt

tui/                 Rust terminal client
  src/               App state, modal input, viewport, ratatui widgets
  Cargo.toml
  build.rs           Cap'n Proto codegen

tests/core/          C++ tests (Google Test)
nix/                 Nix packaging
exercises/           Standalone learning exercises
```

## Design Decisions

**Why Neovim's model?** Neovim proved that a headless core with RPC-attached UIs produces an exceptionally extensible system. The same model applies to a browser.

**Why Cap'n Proto?** Typed schemas, zero-copy serialization, promise pipelining, bidirectional RPC, and code generation for C++ and Rust. The schema IS the API documentation.

**Why Rust for the frontend?** Memory safety, async I/O (tokio), ratatui for TUI, Cap'n Proto support. No CEF dependency.

**Why shared memory for frames?** 1920x1080 at 32bpp is ~8MB per frame, ~240MB/s at 30fps. Shared memory avoids serializing pixels through RPC.

**Why LuaJIT?** Proven by Neovim for exactly this use case. Fast, embeddable, works with CEF's `-fno-exceptions` build.

**Why kitty graphics?** Most capable terminal graphics protocol: raw pixels, RGBA, shared memory mode, image replacement. Supported by kitty, WezTerm, Ghostty.
