# cef-terminal

A terminal-based web browser built on the Chromium Embedded Framework (CEF). CEF handles page loading, JavaScript execution, and networking. We take its rendered pixel buffers and display them in a terminal using the [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).

> **Status: Early development.** The core plumbing works (IPC, command dispatch, runtime exports, CEF lifecycle), but there is no visual output yet. You cannot browse the web with this today. If that sounds like a fun problem to help solve, see [CONTRIBUTING.md](CONTRIBUTING.md).

## What it will be

- A keyboard-driven, modal browser (normal/insert/command modes, inspired by nvim)
- Tabs as buffers, managed by a daemon backend that multiple terminal frontends can attach to
- A self-documenting command surface built from runtime exports — every component declares what it offers, and the system is introspectable at runtime
- Extensible via embedded Lua plugins that compose behavior from the export surface (and eventually IPC-based or FFI plugins)
- Linux-first, designed for developers and terminal power users

## Architecture at a glance

```
cef-backend (daemon)              cef-frontend (terminal client)
  CEF Engine (offscreen render)     Kitty Renderer (pixel display)
  API Dispatcher (export registry)  Input Capture (keyboard/mouse)
  Plugin Runtime (Lua)              Layout (status bar, tabs, etc.)
         \                         /
          --- Unix domain socket ---
```

Backend and frontend are separate OS processes connected via IPC. The frontend never links against CEF. Each component declares its capabilities as **runtime exports** (commands, queries, events, properties) which the dispatcher collects and makes available system-wide. See [ARCHITECTURE.md](ARCHITECTURE.md) for the full design rationale.

## What works today

- **Backend:** CEF initializes in offscreen mode, creates a browser, navigates pages, tracks titles
- **IPC:** Binary-framed messages over Unix domain sockets with typed command/query dispatch
- **Runtime exports:** Components declare structured manifests of commands, queries, events, and properties. The dispatcher indexes them by namespace and provides introspection via `registry.list` and `registry.describe`
- **API layer:** The `buffer.*` namespace exposes navigation commands, title queries, and property access — all declared via export manifest
- **Frontend:** Connects to the backend, sends commands, receives query results
- **Plugin skeleton:** Lua 5.4 runtime initializes and exposes a bridge table (wiring incomplete)
- **Tests:** Serialization round-trips, export registration, introspection, property auto-generation

## What doesn't work yet

- No pixel delivery from backend to frontend (OnPaint capture is stubbed)
- No kitty graphics rendering
- No terminal input handling
- No modal UI, tab management, or command bar
- No event subscription system (events are declared but not yet wired)
- Plugin system can't dispatch commands/queries yet

## Requirements

- Linux (developed on NixOS)
- A terminal supporting the [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) (kitty, WezTerm, Ghostty)
- Nix with flakes enabled (recommended), or CMake 3.21+, Ninja, and the dependencies listed in `flake.nix`

## Quick start (Nix)

```bash
git clone <repo-url> && cd cef-terminal
nix develop          # enters devshell, assembles workspace
build-cef            # cmake + ninja
cd workspace/build/src/Release
./cef-backend --no-sandbox &
./cef-frontend
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full development setup.

## License

TBD
