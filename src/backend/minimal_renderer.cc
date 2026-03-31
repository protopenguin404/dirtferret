#include "include/cef_browser.h"
#include "include/cef_render_handler.h"

#include "minimal_renderer.h"
#include <iostream>

// Minimal render handler for offscreen rendering.
// Returns a 1x1 viewport for now — will be replaced with kitty graphics output.

void RenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) {
  rect = CefRect(0, 0, 800, 600);
}

void RenderHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type, const RectList &dirtyRects,
                            const void *buffer, int width, int height) {
  // Will be replaced with kitty graphics protocol output.
  // For now, just acknowledge we received a paint.
}
