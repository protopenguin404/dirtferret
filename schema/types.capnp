@0xa1b2c3d4e5f60001;

# Shared types for the dirtferret API.
# Used by both core.capnp and ui.capnp.

struct BufferInfo {
  id           @0 :Int32;
  url          @1 :Text;
  title        @2 :Text;
  loading      @3 :Bool;
  canGoBack    @4 :Bool;
  canGoForward @5 :Bool;
  loadProgress @6 :Float64;   # 0.0 to 1.0
}

struct Rect {
  x      @0 :Int32;
  y      @1 :Int32;
  width  @2 :UInt32;
  height @3 :UInt32;
}

struct KeyEvent {
  type       @0 :KeyEventType;
  keyCode    @1 :UInt32;   # Platform-independent virtual key code
  character  @2 :UInt32;   # Unicode codepoint
  modifiers  @3 :UInt32;   # Bitmask: SHIFT=1, CTRL=2, ALT=4, META=8
}

enum KeyEventType {
  rawKeyDown @0;
  keyDown    @1;
  keyUp      @2;
  char       @3;
}

struct MouseEvent {
  type      @0 :MouseEventType;
  x         @1 :Int32;
  y         @2 :Int32;
  button    @3 :MouseButton;
  modifiers @4 :UInt32;
}

enum MouseEventType {
  down       @0;
  up         @1;
  move       @2;
  wheel      @3;
}

enum MouseButton {
  left   @0;
  middle @1;
  right  @2;
  none   @3;
}

enum PixelFormat {
  bgra @0;   # CEF's native output (OnPaint)
  rgba @1;   # Kitty graphics protocol input
}

enum CursorType {
  pointer     @0;
  cross       @1;
  hand        @2;
  ibeam       @3;
  wait        @4;
  help        @5;
  notAllowed  @6;
  grab        @7;
  grabbing    @8;
}

enum LogLevel {
  debug   @0;
  info    @1;
  warning @2;
  error   @3;
}
