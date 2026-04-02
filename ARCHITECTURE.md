# Architecture

This document describes the design of cef-terminal: the process model, layer responsibilities, communication patterns, and key terminology. It is the founding vision document. Implementation details change; the principles here should not (or at least, not without good reason).

## Design principles

- **Separation of concerns with an iron fist.** Each layer has one job. Cross-layer communication goes through the API dispatcher, never through direct calls.
- **Composition over inheritance.** Except where CEF requires subclassing, prefer composable interfaces and dependency injection.
- **Declarative and explicit.** Borrowing philosophically from Nix: reproducible, declarative configuration. No magic globals, no implicit state.
- **Policy vs. mechanics.** If the user might want to configure it, it's policy and lives in the plugin layer. If it's internal coordination, it's mechanical and lives in the API layer.

## The four layers

```
cef-backend (daemon process)
  +------------------+     +-----------------+     +------------------+
  |   CEF Engine     |     |   API Layer     |     |  Plugin Runtime  |
  |                  |     |   (Dispatcher)  |     |  (Lua host)      |
  |  Buffer lifecycle|<--->|  Command/Query  |<--->|  Policy logic    |
  |  OnPaint capture |     |  routing hub    |     |  Default plugins |
  |  CEF wrangling   |     |                 |     |  User plugins    |
  +------------------+     +-----------------+     +------------------+
                                   ^
                                   | IPC (Unix domain socket)
                                   v
                           +-----------------+
                           |    Frontend     |
                           |                 |
                           |  Kitty renderer |
                           |  Input capture  |
                           |  Layout/chrome  |
                           +-----------------+
                       cef-frontend (terminal client)
```

### Layer 1: CEF Engine

The backend's interface to Chromium. Manages browser instances (buffers), handles CEF lifecycle, captures rendered frames via offscreen rendering (OSR). This is the only layer that touches CEF headers.

Responsibilities:
- CEF initialization, message loop, shutdown
- Browser creation and destruction
- Frame capture from `OnPaint` callbacks
- Registering `buffer.*` command/query handlers with the dispatcher

Does NOT handle: tab management UI, user preferences, plugin coordination, input interpretation.

### Layer 2: API Layer (Dispatcher)

The central routing hub. All cross-layer communication flows through here. The dispatcher maintains a registry of command and query handlers, registered by whichever layer owns them.

Responsibilities:
- Command dispatch (fire-and-forget actions with metadata args)
- Query dispatch (read-only requests that return data)
- Handler registration (any layer can register handlers for its namespace)

Commands and queries are typed objects, not raw strings. They can be composed, queued, intercepted, and wrapped by plugins. This is what makes the system extensible without everything knowing about everything else.

The dispatcher is one-per-process, passed by reference to layers that need to register or invoke handlers. The frontend process has its own dispatcher for frontend-local commands (mode switching, viewport scrolling, etc.) that never cross IPC.

### Layer 3: Plugin Runtime

All policy logic lives here. "Policy" means anything the user might want to configure: keybindings, default URLs, tab behavior, search providers, UI layout rules.

Responsibilities:
- Embedding Lua 5.4 as the primary plugin engine
- Loading default and user plugins
- Bridging Lua calls to/from the C++ dispatcher
- Providing a `cef` bridge table for plugins to register handlers and emit commands/queries

Future: IPC-based plugins (external processes), C++ FFI plugins.

### Layer 4: Frontend

The terminal client. Renders pixel buffers via the kitty graphics protocol, captures keyboard and mouse input, manages terminal-side chrome (status bar, tab line, command bar).

Responsibilities:
- Kitty graphics protocol rendering (BGRA to RGBA conversion, base64 encoding, escape sequences)
- Terminal state management (raw mode, alternate screen, cursor, resize)
- Input capture and translation to commands
- Modal UI (normal, insert, command modes)

The frontend does NOT link against CEF. This is enforced at the build level (`target_link_libraries` does not include CEF for the frontend target). Any data the frontend needs from CEF travels over IPC.

## Process model

```
cef-backend (long-running daemon)
  |
  |--- CEF child processes (renderer, GPU, utility — managed by CEF, not us)
  |
  |=== Unix domain socket ===> cef-frontend (terminal client, one per terminal)
  |=== Unix domain socket ===> cef-frontend (another terminal window)
  |=== Unix domain socket ===> cef-frontend (reattached session)
```

- Backend and frontend are **separate OS processes**. Non-negotiable.
- The backend is a **multi-client server**. Multiple frontends can connect simultaneously. This enables detach/reattach (like tmux) and multiple terminal windows viewing the same browser session.
- CEF itself forks child processes for its renderer, GPU, and utility processes. These are handled by `CefExecuteProcess()` at the very start of `main()` and are not part of our architecture.

## IPC

An abstract `Transport` interface (`src/ipc/transport.h`) decouples the mechanism from the rest of the code.

Current implementation: Unix domain sockets with a binary wire protocol.

Wire format:
```
[payload_len:u32][type:u8][id:u32][payload:bytes]
```

Message types: `COMMAND`, `QUERY`, `COMMAND_RESULT`, `QUERY_RESULT`, `FRAME`.

Commands and queries are serialized using a length-prefixed, little-endian binary format with tagged variant values (`src/ipc/serialization.cc`). FRAME messages carry raw pixel data in the payload with additional metadata (width, height, buffer_id).

Future: shared memory transport for zero-copy frame delivery.

## Terminology

| Term | Meaning |
|---|---|
| **Buffer** | One CEF browser instance plus its metadata. Equivalent to a vim buffer. |
| **Policy** | Logic that could be user preference. Lives in the plugin layer. |
| **Mechanics** | Internal coordination logic. Lives in the API layer. |
| **Command** | A fire-and-forget action that crosses layer boundaries. Has a name and optional args. |
| **Query** | A read-only request that crosses layer boundaries. Returns data without side effects. |

## File organization

```
src/
  backend_main.cc           Backend entry point
  frontend_main.cc          Frontend entry point
  backend/                  CEF engine (app, client, renderer, engine)
  api/                      Dispatcher + command/query type definitions
  ipc/                      Transport abstraction + Unix socket + serialization
  plugin/                   Plugin host abstraction + Lua runtime
  frontend/                 Kitty renderer, terminal management, input, layout
  tests/                    Google Test files
```

Rule: one header + implementation pair per major component. CEF headers are only allowed in `src/backend/`.

## Design decisions and rationale

**Why separate processes for backend and frontend?**
Decoupling rendering from display allows detach/reattach, multiple views, and crash isolation. The frontend is lightweight — if it crashes, the backend and all browser state survive.

**Why a command/query dispatcher instead of direct function calls?**
Extensibility. Plugins need to intercept, wrap, and register handlers without modifying core code. Typed command/query objects are composable and serializable. The dispatcher is the single point where policy and mechanics meet.

**Why Lua for plugins?**
Lightweight, embeddable, well-understood. nvim proved the model works for this kind of application. Lua's simplicity is a feature — it keeps plugins focused on policy rather than reimplementing the browser.

**Why offscreen rendering?**
We don't want X11/GTK windows. The terminal is our display. CEF's OSR mode gives us raw pixel buffers that we can send anywhere — in our case, encoded as kitty graphics protocol escape sequences.

**Why kitty graphics protocol specifically?**
It's the most capable terminal graphics protocol available: supports direct pixel data, image placement, and animation. Supported by kitty, WezTerm, Ghostty, and others. Sixel is an alternative but has lower resolution and fewer features.
