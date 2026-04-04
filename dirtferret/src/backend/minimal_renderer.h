#include "include/cef_browser.h"
#include "include/cef_render_handler.h"
#include <iostream>

// Minimal render handler for offscreen rendering.
// Returns a 1x1 viewport for now — will be replaced with kitty graphics output.
class RenderHandler : public CefRenderHandler {
public:
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;

  void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
               const RectList &dirtyRects, const void *buffer, int width,
               int height) override;

private:
  IMPLEMENT_REFCOUNTING(RenderHandler);
};
