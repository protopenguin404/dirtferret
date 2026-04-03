# Architecture

This document describes the design of cef-terminal: the process model, layer responsibilities, communication patterns, and key terminology. It is the founding vision document. Implementation details change; the principles here should not (or at least, not without good reason).

## Design principles

- **Separation of concerns with an iron fist.** Each layer has one job. Cross-layer communication goes through the API dispatcher, never through direct calls.
- **Composition over inheritance.** Except where CEF requires subclassing, prefer composable interfaces and dependency injection.
- **Declarative and explicit.** Borrowing philosophically from Nix: reproducible, declarative configuration. No magic globals, no implicit state.
- **Policy vs. mechanics.** If the user might want to configure it, it's policy and lives in the plugin layer. If it's internal coordination, it's mechanical and lives in the API layer.
- **Components declare their surface.** Every major component declares what it offers to the system as a structured manifest of runtime exports. The system's command surface is emergent, not centrally authored.

## The four layers

```
cef-backend (daemon process)
  +------------------+     +-----------------+     +------------------+
  |   CEF Engine     |     |   API Layer     |     |  Plugin Runtime  |
  |                  |     |   (Dispatcher)  |     |  (Lua host)      |
  |  Buffer lifecycle|<--->|  Export registry |<--->|  Policy logic    |
  |  OnPaint capture |     |  Command/Query  |     |  Default plugins |
  |  CEF wrangling   |     |  routing hub    |     |  User plugins    |
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
- Declaring its runtime exports (the `buffer.*` namespace) via an `ExportManifest`

Does NOT handle: tab management UI, user preferences, plugin coordination, input interpretation.

### Layer 2: API Layer (Dispatcher + Export Registry)

The central routing hub. All cross-layer communication flows through here. The dispatcher collects runtime exports from components, indexes them by fully-qualified name, and provides dispatch and introspection.

Responsibilities:
- Collecting `ExportManifest` declarations from all components
- Command dispatch (fire-and-forget actions with metadata args)
- Query dispatch (read-only requests that return data)
- Property access (auto-generated get/set pairs from property exports)
- Introspection (any consumer can discover the full command surface at runtime)

The dispatcher does not define the command vocabulary — it indexes it. Each component declares what it offers; the dispatcher aggregates. This means adding new capabilities to the system requires changes only in the component that owns them.

Commands, queries, events, and properties are typed objects, not raw strings. They can be composed, queued, intercepted, and wrapped by plugins. This is what makes the system extensible without everything knowing about everything else.

The dispatcher is one-per-process, passed by reference to layers that need to register or invoke handlers. The frontend process has its own dispatcher for frontend-local exports (mode switching, viewport scrolling, etc.) that never cross IPC.

### Layer 3: Plugin Runtime

All policy logic lives here. "Policy" means anything the user might want to configure: keybindings, default URLs, tab behavior, search providers, UI layout rules.

The Plugin Runtime is the **control flow layer**. Runtime exports provide the vocabulary — commands, queries, events, properties — but they don't define how those pieces compose into behavior. That's what plugins do. A plugin might listen to `buffer.load_finished` events and conditionally dispatch `buffer.navigate` commands based on URL patterns. The exports are the nouns and verbs; plugins write the sentences.

Responsibilities:
- Embedding Lua 5.4 as the primary plugin engine
- Loading default and user plugins
- Bridging Lua calls to/from the C++ dispatcher
- Providing a `cef` bridge table for plugins to register handlers and emit commands/queries
- Declaring its own runtime exports (the `plugin.*` namespace) for plugin management
- Subscribing to events from other components and composing behavior from the export surface

Future: IPC-based plugins (external processes), C++ FFI plugins.

### Layer 4: Frontend

The terminal client. Renders pixel buffers via the kitty graphics protocol, captures keyboard and mouse input, manages terminal-side chrome (status bar, tab line, command bar).

Responsibilities:
- Kitty graphics protocol rendering (BGRA to RGBA conversion, base64 encoding, escape sequences)
- Terminal state management (raw mode, alternate screen, cursor, resize)
- Input capture and translation to commands
- Modal UI (normal, insert, command modes)
- Declaring its own runtime exports (the `frontend.*` namespace) for terminal state and input

The frontend does NOT link against CEF. This is enforced at the build level (`target_link_libraries` does not include CEF for the frontend target). Any data the frontend needs from CEF travels over IPC.

## Runtime exports

The system's command surface is built from **runtime exports** — structured declarations that each component makes about what it offers to the rest of the system. Think of it as an access modifier beyond `public`: a component's runtime exports are the subset of its functionality that it deliberately exposes to the broader architecture.

### Export types

Each component declares an `ExportManifest` containing some or all of these:

| Export type | What it is | Example |
|---|---|---|
| **Command** | Fire-and-forget action that may modify state | `buffer.navigate`, `buffer.close` |
| **Query** | Read-only request that returns data | `buffer.get_title`, `buffer.list` |
| **Event** | Notification the component emits | `buffer.load_finished`, `buffer.title_changed` |
| **Property** | Observable state with get (and optionally set) | `buffer.title` (read-only), `buffer.zoom_level` (read-write) |
| **Reference** | Pointer to a related export namespace | `buffer.renderer` → the renderer's exports |
| **Schema/metadata** | Argument types, descriptions, docs | Auto-generated from the above |

### How it works

1. Each component builds an `ExportManifest` with a namespace (e.g., `"buffer"`), a description, and vectors of export declarations.
2. Each export carries its handler (for commands/queries/properties), argument specs, return specs, and documentation.
3. The component hands the manifest to the dispatcher via `register_exports()`.
4. The dispatcher indexes handlers by fully-qualified name (`namespace.name`), auto-generates property getters/setters, and stores the manifest for introspection.
5. Any consumer — another component, a plugin, the frontend over IPC, a debug tool — can dispatch by name or query the registry to discover what exists.

### Properties

Properties are a convenience that collapses a query+command pair into a single declaration. Declaring a property named `"title"` in the `"buffer"` namespace auto-registers:
- `buffer.get_title` (query) — the getter
- `buffer.set_title` (command) — the setter, only if `writable == true`

This reduces boilerplate and keeps the intent clear: this is observable state, not an action.

### Introspection

The dispatcher automatically registers `registry.*` queries:

- `registry.list` — returns all namespaces with their descriptions and export counts
- `registry.describe` — given a namespace, returns full details: every command, query, event, property, and reference with their argument specs and documentation

This makes the entire system self-documenting. A plugin can discover what commands exist. A debug CLI can list all available operations. Auto-generated documentation stays in sync with the code because the documentation *is* the code.

### Why this design

The typical approach is to define all commands in a central routing table and wire them to handlers. That creates a coupling bottleneck — every new capability requires edits in two places (the registry and the handler). With runtime exports, the component is the single source of truth. The registry is just an aggregation pass.

This gives us:
- **Locality** — each component owns its export declarations alongside its implementation
- **Namespacing** — falls out naturally from component identity
- **Typed** — argument/return types are declared in the manifest
- **Introspectable** — any consumer can query what exists at runtime
- **Language-agnostic** — the manifest is data, consumable from Lua, over IPC, or from a debug tool
- **Composable** — plugins operate on the export surface without knowing implementation details

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

Runtime export manifests are serializable by design — the `registry.describe` query returns all metadata as a flat key-value map that travels over the same wire format. This means the frontend (or any IPC client) can introspect the backend's full command surface without special mechanisms.

Future: shared memory transport for zero-copy frame delivery.

## Terminology

| Term | Meaning |
|---|---|
| **Buffer** | One CEF browser instance plus its metadata. Equivalent to a vim buffer. |
| **Runtime export** | A capability that a component deliberately exposes to the system: a command, query, event, property, or reference. |
| **Export manifest** | The structured declaration of all runtime exports for one component. Carries namespace, descriptions, handlers, and argument specs. |
| **Namespace** | The component identity prefix for export names. E.g., `buffer`, `frontend`, `plugin`. |
| **Command** | A fire-and-forget action that may modify state. Has a name and optional args. |
| **Query** | A read-only request that returns data without side effects. |
| **Event** | A notification emitted by a component. Other components or plugins can subscribe. |
| **Property** | Observable state owned by a component. Auto-generates getter/setter query/command pairs. |
| **Reference** | A declared relationship between export namespaces. |
| **Policy** | Logic that could be user preference. Lives in the plugin layer. |
| **Mechanics** | Internal coordination logic. Lives in the API layer. |
| **Introspection** | The ability to query the system at runtime for what exports exist, their types, args, and documentation. |

## File organization

```
src/
  backend_main.cc           Backend entry point
  frontend_main.cc          Frontend entry point
  backend/                  CEF engine (app, client, renderer, engine)
  api/                      Dispatcher, export types, command/query definitions
  ipc/                      Transport abstraction + Unix socket + serialization
  plugin/                   Plugin host abstraction + Lua runtime
  frontend/                 Kitty renderer, terminal management, input, layout
  tests/                    Google Test files
  exercises/                Standalone learning exercises
```

Rule: one header + implementation pair per major component. CEF headers are only allowed in `src/backend/`.

## Design decisions and rationale

**Why separate processes for backend and frontend?**
Decoupling rendering from display allows detach/reattach, multiple views, and crash isolation. The frontend is lightweight — if it crashes, the backend and all browser state survive.

**Why runtime exports instead of a central command registry?**
Locality. Each component is the single source of truth for what it offers. No two-place edits, no central bottleneck. The dispatcher aggregates — it doesn't define. This also makes the system self-documenting: the export declarations carry their own argument specs and descriptions, so introspection and documentation are always in sync with the code.

**Why a command/query dispatcher instead of direct function calls?**
Extensibility. Plugins need to intercept, wrap, and register handlers without modifying core code. Typed command/query objects are composable and serializable. The dispatcher is the single point where policy and mechanics meet.

**Why Lua for plugins?**
Lightweight, embeddable, well-understood. nvim proved the model works for this kind of application. Lua's simplicity is a feature — it keeps plugins focused on policy rather than reimplementing the browser.

**Why offscreen rendering?**
We don't want X11/GTK windows. The terminal is our display. CEF's OSR mode gives us raw pixel buffers that we can send anywhere — in our case, encoded as kitty graphics protocol escape sequences.

**Why kitty graphics protocol specifically?**
It's the most capable terminal graphics protocol available: supports direct pixel data, image placement, and animation. Supported by kitty, WezTerm, Ghostty, and others. Sixel is an alternative but has lower resolution and fewer features.
