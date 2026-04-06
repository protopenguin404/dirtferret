#pragma once

#include "include/cef_render_handler.h"

#include <functional>

// CefRenderHandler for offscreen rendering.
// Captures pixel data from OnPaint and forwards it via a callback.
class Renderer : public CefRenderHandler {
 public:
  using PaintCallback = std::function<void(const void* buffer,
                                           int width, int height,
                                           const RectList& dirtyRects)>;

  void set_paint_callback(PaintCallback cb) { paint_callback_ = std::move(cb); }

  // Set the viewport size. Called when the terminal resizes.
  void set_view_size(int width, int height);

  // --- CefRenderHandler ---
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width, int height) override;

 private:
  PaintCallback paint_callback_;
  int view_width_ = 800;
  int view_height_ = 600;
  IMPLEMENT_REFCOUNTING(Renderer);
};
