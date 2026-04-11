@0xa1b2c3d4e5f60002;

using Types = import "types.capnp";

# The Core interface — the browser kernel's API.
# Implemented by dirtferret-core (C++ server).
# Consumed by dirtferret-tui (Rust client) and external tools.

interface Core {
  # ---- Buffer lifecycle ----
  createBuffer  @0 (url :Text) -> (bufferId :Int32);
  closeBuffer   @1 (bufferId :Int32) -> ();
  listBuffers   @2 () -> (buffers :List(Types.BufferInfo));
  getBufferInfo @3 (bufferId :Int32) -> (info :Types.BufferInfo);

  # ---- Navigation ----
  navigate  @4  (bufferId :Int32, url :Text) -> ();
  goBack    @5  (bufferId :Int32) -> ();
  goForward @6  (bufferId :Int32) -> ();
  reload    @7  (bufferId :Int32) -> ();
  stopLoad  @8  (bufferId :Int32) -> ();

  # ---- Active buffer ----
  setActiveBuffer @9  (bufferId :Int32) -> ();
  getActiveBuffer @10 () -> (bufferId :Int32);

  # ---- Input forwarding (TUI -> CEF) ----
  sendKeyEvent    @11 (bufferId :Int32, event :Types.KeyEvent) -> ();
  sendMouseEvent  @12 (bufferId :Int32, event :Types.MouseEvent) -> ();
  sendScrollEvent @13 (bufferId :Int32, deltaX :Int32, deltaY :Int32) -> ();

  # ---- Viewport ----
  resize @14 (bufferId :Int32, width :UInt32, height :UInt32) -> ();

  # ---- JavaScript ----
  executeScript @15 (bufferId :Int32, code :Text) -> ();

  # ---- Find ----
  find     @16 (bufferId :Int32, text :Text, forward :Bool, matchCase :Bool) -> ();
  stopFind @17 (bufferId :Int32) -> ();

  # ---- Zoom ----
  setZoom  @18 (bufferId :Int32, level :Float64) -> ();
  getZoom  @19 (bufferId :Int32) -> (level :Float64);

  # ---- UI registration ----
  # The TUI calls this on connect, passing its Ui callback capability.
  # The core uses the Ui capability to push frame data and state updates.
  attachUi @20 (ui :Ui, width :UInt32, height :UInt32) -> ();

  # ---- Introspection ----
  getApiSchema @21 () -> (schema :Data);

  # ---- Keybind resolution ----
  # TUI sends keypress, core checks Lua keymap, returns action.
  resolveKeybind @22 (mode :Text, keyCode :UInt32, character :UInt32,
                       modifiers :UInt32) -> (action :Text, arg :Text);
}

# The UI callback interface — implemented by dirtferret-tui.
# The core calls into this to deliver frames and state updates.
# This is analogous to Neovim's "redraw" notification stream.

interface Ui {
  # ---- Frame delivery via shared memory ----
  # shmName is the POSIX shared memory segment name (e.g., "/dirtferret-frame-0").
  # The TUI opens it with shm_open(), mmaps it, reads pixels.
  onFrame @0 (bufferId :Int32, shmName :Text,
              width :UInt32, height :UInt32,
              format :Types.PixelFormat,
              dirtyRects :List(Types.Rect)) -> ();

  # ---- Buffer state updates ----
  onBufferCreated       @1 (info :Types.BufferInfo) -> ();
  onBufferClosed        @2 (bufferId :Int32) -> ();
  onTitleChanged        @3 (bufferId :Int32, title :Text) -> ();
  onUrlChanged          @4 (bufferId :Int32, url :Text) -> ();
  onLoadingStateChanged @5 (bufferId :Int32, loading :Bool,
                            canGoBack :Bool, canGoForward :Bool) -> ();
  onLoadProgress        @6 (bufferId :Int32, progress :Float64) -> ();

  # ---- Input hints ----
  # editable=true means a text field has focus — TUI can auto-enter insert mode.
  onFocusedFieldChanged @7 (bufferId :Int32, editable :Bool) -> ();
  onCursorChanged       @8 (bufferId :Int32, cursorType :Types.CursorType) -> ();

  # ---- Console ----
  onConsoleMessage @9 (bufferId :Int32, level :Types.LogLevel,
                       message :Text, source :Text, line :UInt32) -> ();
}
