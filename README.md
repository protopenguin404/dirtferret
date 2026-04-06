# dirtferret

A terminal-based web browser built on CEF (Chromium Embedded Framework). CEF handles page rendering, JavaScript, and networking. We display its pixel output in any terminal that supports the [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).

The architecture follows [Neovim's model](https://neovim.io/doc/user/develop.html): a headless C++ core exposes a typed [Cap'n Proto](https://capnproto.org/) RPC interface. Terminal UIs, plugins, and external tools all consume the same API.

> **Status: Early development.** The CEF engine initializes and manages browser buffers. Frame delivery, terminal rendering, and input handling are in progress. You cannot browse the web with this yet.

## Architecture

```
dirtferret-core (C++ daemon)           dirtferret-tui (Rust terminal client)
  CEF Engine (offscreen render)          ratatui (layout, widgets, chrome)
  Cap'n Proto RPC server                 Kitty graphics (pixel display)
  Lua runtime (plugins, config)          Modal input (normal/insert/command)
         \                              /
          === Cap'n Proto RPC (Unix socket) ===
```

- **Core** is a long-running daemon that owns all browser state. Multiple UIs can connect.
- **TUI** is a lightweight Rust client using ratatui. Renders pixels via kitty graphics, captures input.
- **API** is defined in `.capnp` schema files — the single source of truth for the entire interface.
- **Frames** travel via shared memory (zero-copy). Cap'n Proto carries only metadata.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full design document.

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Browser engine | CEF 142 (Chromium 142) |
| Core | C++17 |
| Frontend | Rust (ratatui + crossterm) |
| RPC | Cap'n Proto |
| Scripting | LuaJIT |
| Graphics | Kitty protocol |
| Build | CMake + Cargo + Nix |

## Requirements

- Linux (developed on NixOS)
- Nix with flakes enabled
- A terminal supporting the [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) (kitty, WezTerm, Ghostty)

## Quick Start

```bash
git clone <repo-url> && cd dirtferret
nix develop

# Build C++ core
build-core

# Build Rust TUI
build-tui

# Run tests
run-tests
```

## License

TBD
