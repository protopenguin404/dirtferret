# Progress — Keyboard UX (2026-04-13)

Branch: `feat/keyboard-ux` (also `feat/region-dom-bridge` — same commit)
Not merged to main yet.

## What was built

### Layer 1: Region Primitives + DomBridge (commit cb6f939)
- `RegionSet` — unified cursor/selection model (Sublime pattern), 17 tests
- `DomBridge` — CDP wrapper: element_at, query, bounds, text, highlight_node, cursor visual injection
- Engine facade: 8 RPC methods (elementAt, query, regionAdd/Remove/Move/Select/Clear, getRegions)
- All driven from C++ via Chrome DevTools Protocol, no JS codebase

### Layer 2: Cursor Navigation + Commands (commit a6a98f3)
- `CursorState` + `MatchList` in BufferState — cursor engine with cached focusable elements
- Spatial navigation algorithm (`find_nearest_in_direction`), 12 tests
- 10 new RPC methods: cursorInit/Next/Prev/MoveDir/Activate/Clear, matchSet/Next/Prev/Clear
- TUI modes: cursor (c), command (:), normal (n), passthrough
- Async `dispatch_action` with 17+ action handlers
- Command buffer: `:goto <URL>`, `:query <selector>`, `o`, `/`, `f`
- Lua keybinds for cursor mode (h/j/k/l, Tab, Enter, n/N) and command triggers

## What works
- Tab enters cursor mode, highlights first focusable element
- Tab/h/j/k/l cycle/navigate between elements
- `:` enters command mode with text input in status bar
- `o` pre-fills `:goto `, `/` pre-fills `:query `
- `f` highlights all links for following
- n/N cycle through query/link matches
- Esc clears cursor, returns to normal mode
- All 83 tests pass (57 C++ + 26 Rust)

## Known bugs (for next session)

### BUG: cursor-activate clicks wrong location
**Severity:** High
**Symptom:** Pressing Enter on a highlighted element navigates to completely random/unrelated pages. E.g., clicking YouTube video links opens Granger Boxes or H&R Block.
**Likely cause:** The click coordinates (center of element bounds from CDP `DOM.getBoxModel`) may be in the wrong coordinate space. Possibilities:
1. CDP returns bounds in CSS pixels but CEF expects device pixels (or vice versa)
2. CDP returns viewport-relative coords but the page is scrolled (need to add scroll offset)
3. The `getBoxModel` content quad is relative to the document, but `SendMouseClickEvent` expects viewport-relative coordinates
4. The cached element bounds are stale (from a different page/scroll position)
**Where to look:**
- `engine.cc:cursor_activate` — computes click coordinates from `bounds_x/bounds_y`
- `dom_bridge.cc:box_model_to_rect` — converts CDP quad to rect
- `dom_bridge.cc:element_at` — chains getNodeForLocation → getBoxModel → describeNode
- Compare: what does `DOM.getBoxModel` actually return for a visible element? Log the coords and compare to what `SendMouseClickEvent` receives.
**Debug approach:** Add logging in `cursor_activate` to print the element tag, bounds, and click coords. Then compare visually with where the element actually is on the page.

### Other items to investigate
- Shift+Tab for cursor-prev (BackTab key handling in input.rs)
- Visual mode multi-element highlight (currently only highlights one element via Overlay)
- Command mode cursor rendering (block cursor at end of input)
- Schema naming: Rpc prefix inconsistency (cosmetic)

## Architecture reference

```
Lua keybinds → TUI mode.rs → dispatch_action → RPC → Engine facade
                                                         ↓
                                                    CursorState + DomBridge
                                                         ↓
                                                    CDP (DOM/Overlay)
                                                         ↓
                                                    OnPaint → pixels → TUI
```

## Test inventory
- test-region: 17 (RegionSet operations)
- test-schema: 6 (Cap'n Proto round-trips)
- test-input: 10 (key/mouse translation)
- test-shm: 12 (frame pool)
- test-cursor: 12 (spatial navigation algorithm)
- Rust: 26 (mode resolution, mux, layout, shm, region)
