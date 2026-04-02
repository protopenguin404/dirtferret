# Contributing to cef-terminal

This project is in early development. Contributions are welcome, but please read this document first so you understand the build environment and workflow. Things are still being figured out, and some of the ergonomics are rough.

## Development environment

### Nix (recommended)

The easiest way to get a working environment is with [Nix](https://nixos.org/) and flakes enabled:

```bash
nix develop
```

This drops you into a devshell with all dependencies available (CMake, Ninja, CEF binary distribution, Lua 5.4, gtest, X11 libs, etc.) and assembles the `workspace/` directory.

The devshell defines helper commands:

| Command | What it does |
|---|---|
| `build-cef` | Configures (cmake) and builds (ninja) both targets. Output: `workspace/build/src/Release/` |

### Without Nix

If you're not using Nix, you'll need:

- CMake 3.21+
- Ninja
- CEF binary distribution (142.x, Chromium 142.0.7444.135) — download from [cef-builds.spotifycdn.com](https://cef-builds.spotifycdn.com/index.html)
- Lua 5.4 development headers
- Google Test
- System libraries: libx11, nss, nspr, at-spi2-core, cups, libxkbcommon, mesa, glib, dbus, expat, cairo, pango, alsa-lib, and the X11 extension libs (xcomposite, xdamage, xext, xfixes, xrandr, xcb)

You will need to manually set up a workspace that combines CEF's distribution files with our `src/` directory. See the `shellHook` in `flake.nix` for the exact assembly steps — it's essentially symlinking CEF's `include/`, `libcef_dll/`, `cmake/`, `Release/`, and `Resources/` alongside our source tree.

### Known build ergonomics issues

The build setup currently has some friction that we plan to improve:

- **Workspace assembly:** CEF's CMake expects its distribution files and your source tree to coexist in the same directory tree. We work around this by assembling a `workspace/` directory with symlinks. This is functional but confusing for newcomers.
- **Source syncing:** If you're not in the Nix devshell, edits to `src/` need to be visible inside `workspace/src/`. The Nix shell symlinks `src/` directly, but manual setups may need to copy or symlink.
- **CEF's CMakeLists.txt:** The root `CMakeLists.txt` is from CEF's binary distribution with our modifications bolted on. It works, but it's not clean. Longer-term we want a standalone CMake setup that finds CEF as an external package.

If you have CMake expertise and want to help clean this up, that would be a very welcome contribution.

## Project structure

```
flake.nix               Nix devshell definition
CMakeLists.txt          Root CMake config (CEF's, with our target added)
ARCHITECTURE.md         Design rationale and 4-layer model
src/
  backend_main.cc       Backend entry point (daemon process)
  frontend_main.cc      Frontend entry point (terminal client)
  backend/              CEF engine layer (app, client, renderer, engine)
  frontend/             Terminal renderer, input capture, layout
  api/                  Command/query dispatcher and type definitions
  ipc/                  Transport abstraction + Unix socket implementation
  plugin/               Plugin host abstraction + Lua runtime
  tests/                Google Test files
  exercises/            Standalone C++ learning exercises (not part of the browser)
```

**The important rule:** CEF headers (`#include "include/cef_*.h"`) are only allowed in `src/backend/` files. The frontend process does not link against CEF. This is enforced at the build level. If you need data from CEF in the frontend, it must travel over IPC.

## Building and testing

```bash
# Inside nix develop:
build-cef

# Run the backend (must be from the output directory):
cd workspace/build/src/Release
./cef-backend --no-sandbox

# In another terminal, run the frontend:
cd workspace/build/src/Release
./cef-frontend

# Run tests:
cd workspace/build
ninja test-serialization && src/test-serialization    # IPC round-trip tests
ninja test-frame-wire && src/test-frame-wire          # frame wire format tests
ninja test-frontend && src/test-frontend              # terminal + renderer tests
ctest --output-on-failure                             # run all tests
```

The `--no-sandbox` flag is required because CEF's sandbox binary needs SUID root. This is fine for development.

## Architecture overview

Read [ARCHITECTURE.md](ARCHITECTURE.md) for the full design. The short version:

- **4 layers:** CEF engine, API dispatcher, plugin runtime, terminal frontend
- **2 processes:** Backend (daemon) and frontend (terminal client), connected via Unix domain socket IPC
- **Commands** are fire-and-forget actions. **Queries** are read-only information requests. Both are typed objects routed through the dispatcher.
- The backend is a multi-client server. Multiple frontends can connect to the same backend (enables detach/reattach, multiple terminal windows).

## Code conventions

- **C++17** (what CEF's build system uses)
- **snake_case** for our functions/variables, **PascalCase** for classes
- **PascalCase** for CEF overrides (matching their signatures)
- **No exceptions** — CEF builds with `-fno-exceptions`. Use return values for errors.
- **Composition over inheritance** (except where CEF requires subclassing)
- Debug logging to `std::cerr` with prefixes: `[cef]`, `[ipc]`, `[api]`, `[render]`, `[input]`, `[plugin]`
- Prefer `std::string` over `CefString` in our code; convert at the boundary

## Testing

We use Google Test. Tests live in `src/tests/test_*.cc`.

Tests serve as the spec — if a test compiles and passes, the implementation satisfies the requirement. We keep the test set minimal and focused rather than exhaustive. Each test forces one specific requirement.

Tests do **not** link against CEF. We only unit-test our own code.

## What needs help

Roughly in priority order:

1. **Frame delivery pipeline** — capturing pixels from CEF's OnPaint, sending them over IPC, rendering via kitty graphics protocol. This is the critical proof-of-concept milestone.
2. **Terminal input handling** — reading keyboard/mouse events in raw mode, dispatching them as commands.
3. **CMake cleanup** — decoupling our build from CEF's distribution CMakeLists.
4. **Wire format extension** — the IPC transport needs to carry frame metadata (width, height, buffer_id) for FRAME messages.
5. **Lua plugin bridging** — the runtime initializes but can't actually dispatch commands/queries yet.

If you're interested in contributing, open an issue to discuss before writing a large PR. The architecture is intentional and we'd rather discuss design before implementation.
