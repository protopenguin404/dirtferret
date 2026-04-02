# cef-terminal

A terminal-based web browser built on the Chromium Embedded Framework (CEF). CEF handles page loading, JavaScript execution, and networking. We take its rendered pixel buffers and display them in a terminal using the [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).

> **Status: Early development.** The core plumbing works (IPC, command dispatch, CEF lifecycle), but there is no visual output yet. You cannot browse the web with this today. If that sounds like a fun problem to help solve, see [CONTRIBUTING.md](CONTRIBUTING.md).

## What it will be

- A keyboard-driven, modal browser (normal/insert/command modes, inspired by nvim)
- Tabs as buffers, managed by a daemon backend that multiple terminal frontends can attach to
- Extensible via embedded Lua plugins (and eventually IPC-based or FFI plugins)
- Linux-first, designed for developers and terminal power users

## Architecture at a glance

```
cef-backend (daemon)              cef-frontend (terminal client)
  CEF Engine (offscreen render)     Kitty Renderer (pixel display)
  API Dispatcher (command/query)    Input Capture (keyboard/mouse)
  Plugin Runtime (Lua)              Layout (status bar, tabs, etc.)
         \                         /
          --- Unix domain socket ---
```

Backend and frontend are separate OS processes connected via IPC. The frontend never links against CEF. See [ARCHITECTURE.md](ARCHITECTURE.md) for the full design rationale.

## What works today

- **Backend:** CEF initializes in offscreen mode, creates a browser, navigates pages, tracks titles
- **IPC:** Binary-framed messages over Unix domain sockets with typed command/query dispatch
- **API layer:** Dispatcher routes commands (`buffer.navigate`) and queries (`buffer.get_title`) to handlers
- **Frontend:** Connects to the backend, sends commands, receives query results
- **Plugin skeleton:** Lua 5.4 runtime initializes and exposes a bridge table (wiring incomplete)

## What doesn't work yet

- No pixel delivery from backend to frontend (OnPaint capture is stubbed)
- No kitty graphics rendering
- No terminal input handling
- No modal UI, tab management, or command bar
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
